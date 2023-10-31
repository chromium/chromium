// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/spellcheck_dictionary.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "services/network/public/cpp/simple_url_loader.h"

class GURL;
class SpellcheckService;

namespace content {
class BrowserContext;
}  // namespace content

// Defines the browser-side hunspell dictionary and provides access to it.
class SpellcheckHunspellDictionary : public SpellcheckDictionary {
 public:
  // Interface to implement for observers of the Hunspell dictionary.
  class Observer {
   public:
    // The dictionary has been initialized.
    virtual void OnHunspellDictionaryInitialized(
        const std::string& language) = 0;

    // Dictionary download began.
    virtual void OnHunspellDictionaryDownloadBegin(
        const std::string& language) = 0;

    // Dictionary download succeeded.
    virtual void OnHunspellDictionaryDownloadSuccess(
        const std::string& language) = 0;

    // Dictionary download failed.
    virtual void OnHunspellDictionaryDownloadFailure(
        const std::string& language) = 0;
  };

  SpellcheckHunspellDictionary(const std::string& language,
                               const std::string& platform_spellcheck_language,
                               content::BrowserContext* browser_context,
                               SpellcheckService* spellcheck_service);

  SpellcheckHunspellDictionary(const SpellcheckHunspellDictionary&) = delete;
  SpellcheckHunspellDictionary& operator=(const SpellcheckHunspellDictionary&) =
      delete;

  ~SpellcheckHunspellDictionary() override;

  // SpellcheckDictionary implementation:
  void Load() override;

  // Retry downloading |dictionary_file_|.
  void RetryDownloadDictionary(content::BrowserContext* context);

  // Returns true if the dictionary is ready to use.
  virtual bool IsReady() const;

  const base::File& GetDictionaryFile() const;
  const std::string& GetLanguage() const;
  const std::string& GetPlatformSpellcheckLanguage() const;
  bool HasPlatformSupport() const;
  bool IsUsingPlatformChecker() const;

  // Add an observer for Hunspell dictionary events.
  void AddObserver(Observer* observer);

  // Remove an observer for Hunspell dictionary events.
  void RemoveObserver(Observer* observer);

  // Whether dictionary is being downloaded.
  bool IsDownloadInProgress();

  // Whether dictionary download failed.
  bool IsDownloadFailure();

  // Get a WeakPtr to the instance.
  base::WeakPtr<SpellcheckHunspellDictionary> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Tests use this method to set a custom URL for downloading dictionaries.
  static void SetDownloadURLForTesting(const GURL url);

 private:
  // Dictionary download status.
  enum DownloadStatus {
    DOWNLOAD_NONE,
    DOWNLOAD_IN_PROGRESS,
    DOWNLOAD_FAILED,
  };

  // Dictionary file information to be passed between the UI thread and the
  // blocking sequence.
  struct DictionaryFile {
   public:
    explicit DictionaryFile(base::TaskRunner* task_runner);

    DictionaryFile(const DictionaryFile&) = delete;
    DictionaryFile& operator=(const DictionaryFile&) = delete;

    ~DictionaryFile();

    DictionaryFile(DictionaryFile&& other);
    DictionaryFile& operator=(DictionaryFile&& other);

    // The desired location of the dictionary file, whether or not it exists.
    base::FilePath path;

    // The dictionary file.
    base::File file;

   private:
    // Task runner where the file is created.
    scoped_refptr<base::TaskRunner> task_runner_;
  };

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Determine the correct url to download the dictionary.
  GURL GetDictionaryURL();

  // Attempt to download the dictionary.
  void DownloadDictionary(GURL url);

#if !BUILDFLAG(IS_ANDROID)
  // Figures out the location for the dictionary, verifies its contents, and
  // opens it.
  static DictionaryFile OpenDictionaryFile(base::TaskRunner* task_runner,
                                           const base::FilePath& path);

  // Gets the default location for the dictionary file.
  static DictionaryFile InitializeDictionaryLocation(
      base::TaskRunner* task_runner, const std::string& language);

  // The reply point for PostTaskAndReplyWithResult, called after the dictionary
  // file has been initialized.
  void InitializeDictionaryLocationComplete(DictionaryFile file);
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void SpellCheckPlatformSetLanguageComplete(bool result);
#endif

  // The reply point for PostTaskAndReplyWithResult, called after the dictionary
  // file has been saved.
  void SaveDictionaryDataComplete(bool dictionary_saved);

  // Notify listeners that the dictionary has been initialized.
  void InformListenersOfInitialization();

  // Notify listeners that the dictionary download failed.
  void InformListenersOfDownloadFailure();

  // Callback for asynchronously checking if the platform supports a language
  // for spellchecking.
  void PlatformSupportsLanguageComplete(bool platform_supports_language);

  // Task runner where the file operations takes place.
  scoped_refptr<base::SequencedTaskRunner> const task_runner_;

  // The language of the dictionary file (passed when loading Hunspell
  // dictionaries).
  const std::string language_;

  // The spellcheck language passed to platform APIs may differ from the accept
  // language (can be empty, indicating to use accept language and Hunspell).
  const std::string platform_spellcheck_language_;

  // Whether to use the platform spellchecker instead of Hunspell.
  bool use_browser_spellchecker_;

  // Used for downloading the dictionary file. SpellcheckHunspellDictionary does
  // not hold a reference, and it is only valid to use it on the UI thread.
  raw_ptr<content::BrowserContext> browser_context_;

  // Used for downloading the dictionary file.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  const raw_ptr<SpellcheckService> spellcheck_service_;

  // Observers of Hunspell dictionary events.
  base::ObserverList<Observer>::Unchecked observers_;

  // Status of the dictionary download.
  DownloadStatus download_status_;

  // Dictionary file path and descriptor.
  DictionaryFile dictionary_file_;

  base::WeakPtrFactory<SpellcheckHunspellDictionary> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_
