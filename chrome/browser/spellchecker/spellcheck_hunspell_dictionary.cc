// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"

#include <stddef.h>

#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/files/memory_mapped_file.h"
#include "third_party/hunspell/google/bdict.h"  // nogncheck crbug.com/1125897
#endif

using content::BrowserThread;

namespace {

base::LazyInstance<GURL>::Leaky g_download_url_for_testing =
    LAZY_INSTANCE_INITIALIZER;

// Close the file.
void CloseDictionary(base::File file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  file.Close();
}

// Saves |data| to file at |path|. Returns true on successful save, otherwise
// returns false.
bool SaveDictionaryData(std::unique_ptr<std::string> data,
                        const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::WriteFile(path, *data)) {
    bool success = false;
#if BUILDFLAG(IS_WIN)
    base::FilePath dict_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &dict_dir);
    base::FilePath fallback_file_path =
        dict_dir.Append(path.BaseName());
    if (base::WriteFile(fallback_file_path, *data)) {
      success = true;
    }
#endif

    if (!success) {
      base::DeleteFile(path);
      return false;
    }
  }

  return true;
}

}  // namespace

SpellcheckHunspellDictionary::DictionaryFile::DictionaryFile(
    base::TaskRunner* task_runner) : task_runner_(task_runner) {}

SpellcheckHunspellDictionary::DictionaryFile::~DictionaryFile() {
  if (file.IsValid()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&CloseDictionary, std::move(file)));
  }
}

SpellcheckHunspellDictionary::DictionaryFile::DictionaryFile(
    DictionaryFile&& other)
    : path(other.path),
      file(std::move(other.file)),
      task_runner_(std::move(other.task_runner_)) {}

SpellcheckHunspellDictionary::DictionaryFile&
SpellcheckHunspellDictionary::DictionaryFile::operator=(
    DictionaryFile&& other) {
  path = other.path;
  file = std::move(other.file);
  task_runner_ = std::move(other.task_runner_);
  return *this;
}

SpellcheckHunspellDictionary::SpellcheckHunspellDictionary(
    const std::string& language,
    const std::string& platform_spellcheck_language,
    content::BrowserContext* browser_context,
    SpellcheckService* spellcheck_service)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      language_(language),
      platform_spellcheck_language_(platform_spellcheck_language),
      use_browser_spellchecker_(false),
      browser_context_(browser_context),
      spellcheck_service_(spellcheck_service),
      download_status_(DOWNLOAD_NONE),
      dictionary_file_(task_runner_.get()) {}

SpellcheckHunspellDictionary::~SpellcheckHunspellDictionary() {
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Disable the language from platform spellchecker.
  if (spellcheck::UseBrowserSpellChecker() && HasPlatformSupport()) {
    spellcheck_platform::DisableLanguage(
        spellcheck_service_->platform_spell_checker(),
        GetPlatformSpellcheckLanguage());
  }
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)
}

void SpellcheckHunspellDictionary::Load() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (spellcheck::UseBrowserSpellChecker() &&
      spellcheck_platform::SpellCheckerAvailable() && HasPlatformSupport()) {
    spellcheck_platform::PlatformSupportsLanguage(
        spellcheck_service_->platform_spell_checker(),
        GetPlatformSpellcheckLanguage(),
        base::BindOnce(
            &SpellcheckHunspellDictionary::PlatformSupportsLanguageComplete,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
#endif  // USE_BROWSER_SPELLCHECKER

  // Platform spellchecker isn't enabled, so the language is unsupported.
  PlatformSupportsLanguageComplete(false);
}

void SpellcheckHunspellDictionary::RetryDownloadDictionary(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (dictionary_file_.file.IsValid()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  browser_context_ = browser_context;
  DownloadDictionary(GetDictionaryURL());
}

bool SpellcheckHunspellDictionary::IsReady() const {
  return GetDictionaryFile().IsValid() || IsUsingPlatformChecker();
}

const base::File& SpellcheckHunspellDictionary::GetDictionaryFile() const {
  return dictionary_file_.file;
}

const std::string& SpellcheckHunspellDictionary::GetLanguage() const {
  return language_;
}

const std::string& SpellcheckHunspellDictionary::GetPlatformSpellcheckLanguage()
    const {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Currently the platform spellcheck language is only distinguished for
  // Windows.
  return platform_spellcheck_language_;
#else
  return language_;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
}

bool SpellcheckHunspellDictionary::HasPlatformSupport() const {
  return !GetPlatformSpellcheckLanguage().empty();
}

bool SpellcheckHunspellDictionary::IsUsingPlatformChecker() const {
  return use_browser_spellchecker_;
}

void SpellcheckHunspellDictionary::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void SpellcheckHunspellDictionary::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

bool SpellcheckHunspellDictionary::IsDownloadInProgress() {
  return download_status_ == DOWNLOAD_IN_PROGRESS;
}

bool SpellcheckHunspellDictionary::IsDownloadFailure() {
  return download_status_ == DOWNLOAD_FAILED;
}

void SpellcheckHunspellDictionary::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool is_success = simple_loader_->NetError() == net::OK;
  int response_code = -1;
  if (simple_loader_->ResponseInfo() && simple_loader_->ResponseInfo()->headers)
    response_code = simple_loader_->ResponseInfo()->headers->response_code();

  if (!is_success || ((response_code / 100) != 2)) {
    // Initialize will not try to download the file a second time.
    InformListenersOfDownloadFailure();
    return;
  }

  // We don't need the loader anymore.
  simple_loader_.reset();

  // Basic sanity check on the dictionary. There's a small chance of 200 status
  // code for a body that represents some form of failure.
  if (!data || data->size() < 4 || data->compare(0, 4, "BDic") != 0) {
    InformListenersOfDownloadFailure();
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  // To prevent corrupted dictionary data from causing a renderer crash, scan
  // the dictionary data and verify it is sane before save it to a file.
  if (!hunspell::BDict::Verify(base::as_byte_span(*data))) {
    // Let PostTaskAndReply caller send to InformListenersOfInitialization
    // through SaveDictionaryDataComplete().
    SaveDictionaryDataComplete(false);
    return;
  }
#endif

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SaveDictionaryData, std::move(data),
                     dictionary_file_.path),
      base::BindOnce(&SpellcheckHunspellDictionary::SaveDictionaryDataComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SpellcheckHunspellDictionary::SetDownloadURLForTesting(const GURL url) {
  g_download_url_for_testing.Get() = url;
}

GURL SpellcheckHunspellDictionary::GetDictionaryURL() {
  if (g_download_url_for_testing.Get() != GURL())
    return g_download_url_for_testing.Get();

  std::string bdict_file = dictionary_file_.path.BaseName().MaybeAsASCII();
  DCHECK(!bdict_file.empty());

  static const char kDownloadServerUrl[] =
      "https://redirector.gvt1.com/edgedl/chrome/dict/";

  return GURL(std::string(kDownloadServerUrl) +
              base::ToLowerASCII(bdict_file));
}

void SpellcheckHunspellDictionary::DownloadDictionary(GURL url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context_);

  download_status_ = DOWNLOAD_IN_PROGRESS;
  for (Observer& observer : observers_)
    observer.OnHunspellDictionaryDownloadBegin(language_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("spellcheck_hunspell_dictionary", R"(
        semantics {
          sender: "Spellcheck Dictionary Downloader"
          description:
            "When user selects a new language for spell checking in Google "
            "Chrome, a new dictionary is downloaded for it."
          trigger: "User selects a new language for spell checking."
          data:
            "The spell checking language identifier. No user identifier is "
            "sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can prevent downloading dictionaries by not selecting 'Use "
            "this language for spell checking.' in Chrome's settings under "
            "Lanugagues -> 'Language and input settings...'."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  network::mojom::URLLoaderFactory* loader_factory =
      browser_context_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory,
      base::BindOnce(&SpellcheckHunspellDictionary::OnSimpleLoaderComplete,
                     base::Unretained(this)));

  // Attempt downloading the dictionary only once.
  browser_context_ = nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
// static
SpellcheckHunspellDictionary::DictionaryFile
SpellcheckHunspellDictionary::OpenDictionaryFile(base::TaskRunner* task_runner,
                                                 const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // The default_dictionary_file can either come from the standard list of
  // hunspell dictionaries (determined in InitializeDictionaryLocation), or it
  // can be passed in via an extension. In either case, the file is checked for
  // existence so that it's not re-downloaded.
  // For systemwide installations on Windows, the default directory may not
  // have permissions for download. In that case, the alternate directory for
  // download is chrome::DIR_USER_DATA.
  DictionaryFile dictionary(task_runner);

#if BUILDFLAG(IS_WIN)
  // Check if the dictionary exists in the fallback location. If so, use it
  // rather than downloading anew.
  base::FilePath user_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_dir);
  base::FilePath fallback = user_dir.Append(path.BaseName());
  if (!base::PathExists(path) && base::PathExists(fallback))
    dictionary.path = fallback;
  else
    dictionary.path = path;
#else
  dictionary.path = path;
#endif  // BUILDFLAG(IS_WIN)

  // Open the dictionary file and verify there is no corruption. If verification
  // fails the file must be deleted.

  dictionary.file.Initialize(dictionary.path,
                             base::File::FLAG_READ | base::File::FLAG_OPEN);
  if (!dictionary.file.IsValid()) {
    dictionary.file.Close();
    base::DeleteFile(dictionary.path);
    return dictionary;
  }

  std::vector<uint8_t> data;
  data.resize(dictionary.file.GetLength());
  if (!dictionary.file.ReadAndCheck(0, data) ||
      !hunspell::BDict::Verify(data)) {
    dictionary.file.Close();
    base::DeleteFile(dictionary.path);
  }

  return dictionary;
}

// static
SpellcheckHunspellDictionary::DictionaryFile
SpellcheckHunspellDictionary::InitializeDictionaryLocation(
    base::TaskRunner* task_runner, const std::string& language) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // The default place where the spellcheck dictionary resides is
  // chrome::DIR_APP_DICTIONARIES.
  //
  // Initialize the BDICT path. Initialization should be on the blocking
  // sequence because it checks if there is a "Dictionaries" directory and
  // create it.
  base::FilePath dict_dir;
  base::PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);
  base::FilePath dict_path =
      spellcheck::GetVersionedFileName(language, dict_dir);

  return OpenDictionaryFile(task_runner, dict_path);
}

void SpellcheckHunspellDictionary::InitializeDictionaryLocationComplete(
    DictionaryFile file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  dictionary_file_ = std::move(file);

  if (!dictionary_file_.file.IsValid()) {
    // Notify browser tests that this dictionary is corrupted. Skip downloading
    // the dictionary in browser tests.
    // TODO(rouslan): Remove this test-only case.
    if (spellcheck_service_->SignalStatusEvent(
          SpellcheckService::BDICT_CORRUPTED)) {
      browser_context_ = nullptr;
    }

    if (browser_context_) {
      // Download from the UI thread to check that |browser_context_| is
      // still valid.
      DownloadDictionary(GetDictionaryURL());
      return;
    }
  }

  InformListenersOfInitialization();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void SpellcheckHunspellDictionary::SaveDictionaryDataComplete(
    bool dictionary_saved) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (dictionary_saved) {
    download_status_ = DOWNLOAD_NONE;
    for (Observer& observer : observers_)
      observer.OnHunspellDictionaryDownloadSuccess(language_);
    Load();
  } else {
    InformListenersOfDownloadFailure();
    InformListenersOfInitialization();
  }
}

void SpellcheckHunspellDictionary::InformListenersOfInitialization() {
  for (Observer& observer : observers_)
    observer.OnHunspellDictionaryInitialized(language_);
}

void SpellcheckHunspellDictionary::InformListenersOfDownloadFailure() {
  download_status_ = DOWNLOAD_FAILED;
  for (Observer& observer : observers_)
    observer.OnHunspellDictionaryDownloadFailure(language_);
}

void SpellcheckHunspellDictionary::PlatformSupportsLanguageComplete(
    bool platform_supports_language) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (platform_supports_language) {
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    if (spellcheck::UseBrowserSpellChecker() && HasPlatformSupport()) {
      spellcheck_platform::SetLanguage(
          spellcheck_service_->platform_spell_checker(),
          GetPlatformSpellcheckLanguage(),
          base::BindOnce(&SpellcheckHunspellDictionary::
                             SpellCheckPlatformSetLanguageComplete,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    NOTREACHED_IN_MIGRATION();
  } else {
    // Either the platform spellchecker is unavailable / disabled, or it doesn't
    // support this language. In either case, we must use Hunspell for this
    // language, unless we are on Android, which doesn't support Hunspell.
#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_RENDERER_SPELLCHECKER)
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&InitializeDictionaryLocation,
                       base::RetainedRef(task_runner_.get()), language_),
        base::BindOnce(
            &SpellcheckHunspellDictionary::InitializeDictionaryLocationComplete,
            weak_ptr_factory_.GetWeakPtr()));
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  }
}

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellcheckHunspellDictionary::SpellCheckPlatformSetLanguageComplete(
    bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!result)
    return;

  use_browser_spellchecker_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SpellcheckHunspellDictionary::InformListenersOfInitialization,
          weak_ptr_factory_.GetWeakPtr()));
}
#endif
