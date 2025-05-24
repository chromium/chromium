// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_functions_desktop.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "chrome/browser/extensions/chrome_zipfile_installer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/error_map.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/file_highlighter.h"
#include "extensions/browser/path_util.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/ui_util.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/browser/zipfile_installer.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "net/base/filename_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace developer = api::developer_private;

namespace {
constexpr char kUnpackedAppsFolder[] = "apps_target";

// TODO(crbug.com/392777363): Remove this function moving all its usage to
// shared.cc.
std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  // This call can fail, but it doesn't matter for our purposes. If it fails,
  // we simply return an empty string for the manifest, and ignore it.
  std::ignore = base::ReadFileToString(path, &data);
  return data;
}

using GetManifestErrorCallback =
    base::OnceCallback<void(const base::FilePath& file_path,
                            const std::string& error,
                            size_t line_number,
                            const std::string& manifest)>;
// Takes in an |error| string and tries to parse it as a manifest error (with
// line number), asynchronously calling |callback| with the results.
void GetManifestError(const std::string& error,
                      const base::FilePath& extension_path,
                      GetManifestErrorCallback callback) {
  size_t line = 0u;
  size_t column = 0u;
  std::string regex = base::StringPrintf("%s  Line: (\\d+), column: (\\d+), .*",
                                         manifest_errors::kManifestParseError);
  // If this was a JSON parse error, we can highlight the exact line with the
  // error. Otherwise, we should still display the manifest (for consistency,
  // reference, and so that if we ever make this really fancy and add an editor,
  // it's ready).
  //
  // This regex call can fail, but if it does, we just don't highlight anything.
  re2::RE2::FullMatch(error, regex, &line, &column);

  // This will read the manifest and call AddFailure with the read manifest
  // contents.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFileToString,
                     extension_path.Append(kManifestFilename)),
      base::BindOnce(std::move(callback), extension_path, error, line));
}

// Creates a developer::LoadError from the provided data.
developer::LoadError CreateLoadError(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest,
    const DeveloperPrivateAPI::UnpackedRetryId& retry_guid) {
  base::FilePath prettified_path = path_util::PrettifyPath(file_path);

  SourceHighlighter highlighter(manifest, line_number);
  developer::LoadError response;
  response.error = error;
  response.path = base::UTF16ToUTF8(prettified_path.LossyDisplayName());
  response.retry_guid = retry_guid;

  response.source.emplace();
  response.source->before_highlight = highlighter.GetBeforeFeature();
  response.source->highlight = highlighter.GetFeature();
  response.source->after_highlight = highlighter.GetAfterFeature();

  return response;
}

}  // namespace

namespace ChoosePath = api::developer_private::ChoosePath;
namespace PackDirectory = api::developer_private::PackDirectory;
namespace Reload = api::developer_private::Reload;

namespace api {

DeveloperPrivateReloadFunction::DeveloperPrivateReloadFunction() = default;
DeveloperPrivateReloadFunction::~DeveloperPrivateReloadFunction() = default;

ExtensionFunction::ResponseAction DeveloperPrivateReloadFunction::Run() {
  std::optional<Reload::Params> params = Reload::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  reloading_extension_path_ = extension->path();

  bool fail_quietly = false;
  bool wait_for_completion = false;
  if (params->options) {
    fail_quietly =
        params->options->fail_quietly && *params->options->fail_quietly;
    // We only wait for completion for unpacked extensions, since they are the
    // only extensions for which we can show actionable feedback to the user.
    wait_for_completion = params->options->populate_error_for_unpacked &&
                          *params->options->populate_error_for_unpacked &&
                          Manifest::IsUnpackedLocation(extension->location());
  }

  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(browser_context());
  if (fail_quietly) {
    registrar->ReloadExtensionWithQuietFailure(params->extension_id);
  } else {
    registrar->ReloadExtension(params->extension_id);
  }

  if (!wait_for_completion) {
    return RespondNow(NoArguments());
  }

  // Balanced in ClearObservers(), which is called from the first observer
  // method to be called with the appropriate extension (or shutdown).
  AddRef();
  error_reporter_observation_.Observe(LoadErrorReporter::GetInstance());
  registry_observation_.Observe(ExtensionRegistry::Get(browser_context()));

  return RespondLater();
}

void DeveloperPrivateReloadFunction::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->path() == reloading_extension_path_) {
    // Reload succeeded!
    Respond(NoArguments());
    ClearObservers();
  }
}

void DeveloperPrivateReloadFunction::OnShutdown(ExtensionRegistry* registry) {
  Respond(Error("Shutting down."));
  ClearObservers();
}

void DeveloperPrivateReloadFunction::OnLoadFailure(
    content::BrowserContext* browser_context,
    const base::FilePath& file_path,
    const std::string& error) {
  if (file_path == reloading_extension_path_) {
    // Reload failed - create an error to pass back to the extension.
    GetManifestError(
        error, file_path,
        base::BindOnce(&DeveloperPrivateReloadFunction::OnGotManifestError,
                       this));  // Creates a reference.
    ClearObservers();
  }
}

void DeveloperPrivateReloadFunction::OnGotManifestError(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest) {
  DeveloperPrivateAPI::UnpackedRetryId retry_guid =
      DeveloperPrivateAPI::Get(browser_context())
          ->AddUnpackedPath(GetSenderWebContents(), reloading_extension_path_);
  // Respond to the caller with the load error, which allows the caller to retry
  // reloading through developerPrivate.loadUnpacked().
  // TODO(devlin): This is weird. Really, we should allow retrying through this
  // function instead of through loadUnpacked(), but
  // ExtensionRegistrar::ReloadExtension doesn't behave well with an extension
  // that failed to reload, and untangling that mess is quite significant.
  // See https://crbug.com/792277.
  Respond(WithArguments(
      CreateLoadError(file_path, error, line_number, manifest, retry_guid)
          .ToValue()));
}

void DeveloperPrivateReloadFunction::ClearObservers() {
  registry_observation_.Reset();
  error_reporter_observation_.Reset();

  Release();  // Balanced in Run().
}

DeveloperPrivateLoadUnpackedFunction::DeveloperPrivateLoadUnpackedFunction() =
    default;

DeveloperPrivateLoadUnpackedFunction::~DeveloperPrivateLoadUnpackedFunction() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get()) {
    select_file_dialog_->ListenerDestroyed();
  }
}

ExtensionFunction::ResponseAction DeveloperPrivateLoadUnpackedFunction::Run() {
  std::optional<developer::LoadUnpacked::Params> params =
      developer::LoadUnpacked::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile && supervised_user::AreExtensionsPermissionsEnabled(profile)) {
    return RespondNow(
        Error("Child account users cannot load unpacked extensions."));
  }
  PrefService* prefs = profile->GetPrefs();
  if (!prefs->GetBoolean(prefs::kExtensionsUIDeveloperMode)) {
    return RespondNow(
        Error("Must be in developer mode to load unpacked extensions."));
  }
  if (ExtensionManagementFactory::GetForBrowserContext(browser_context())
          ->BlocklistedByDefault()) {
    return RespondNow(Error("Extension installation is blocked by policy."));
  }

  fail_quietly_ = params->options && params->options->fail_quietly &&
                  *params->options->fail_quietly;

  populate_error_ = params->options && params->options->populate_error &&
                    *params->options->populate_error;

  if (params->options && params->options->retry_guid) {
    DeveloperPrivateAPI* api = DeveloperPrivateAPI::Get(browser_context());
    base::FilePath path =
        api->GetUnpackedPath(web_contents, *params->options->retry_guid);
    if (path.empty()) {
      return RespondNow(Error("Invalid retry id"));
    }

    AddRef();  // Balanced in Finish.
    StartFileLoad(path);
    return RespondLater();
  }

  if (params->options && params->options->use_dragged_path &&
      *params->options->use_dragged_path) {
    DeveloperPrivateAPI* api = DeveloperPrivateAPI::Get(browser_context());
    ui::FileInfo file = api->GetDraggedFile(web_contents);
    if (file.path.empty()) {
      return RespondNow(Error("No dragged path"));
    }

    AddRef();  // Balanced in Finish.
    StartFileLoad(file.path);
    return RespondLater();
  }

  ShowSelectFileDialog();
  AddRef();  // Balanced in Finish.
  return RespondLater();
}

void DeveloperPrivateLoadUnpackedFunction::ShowSelectFileDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Start or cancel the file load without showing the select file dialog for
  // tests that require it.
  if (accept_dialog_for_testing_.has_value()) {
    if (accept_dialog_for_testing_.value()) {
      CHECK(selected_file_for_testing_.has_value());
      FileSelected(selected_file_for_testing_.value(), /*index=*/0);
    } else {
      FileSelectionCanceled();
    }
    return;
  }

  content::WebContents* web_contents = GetSenderWebContents();
  CHECK(web_contents);
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  ui::SelectFileDialog::Type file_type =
      ui::SelectFileDialog::SELECT_EXISTING_FOLDER;
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);
  const base::FilePath last_directory =
      DeveloperPrivateAPI::Get(browser_context())->last_unpacked_directory();
  auto file_type_info = ui::SelectFileDialog::FileTypeInfo();
  int file_type_index = 0;
  gfx::NativeWindow owning_window =
      platform_util::GetTopLevel(web_contents->GetNativeView());

  select_file_dialog_->SelectFile(file_type, title, last_directory,
                                  &file_type_info, file_type_index,
                                  base::FilePath::StringType(), owning_window);
}

void DeveloperPrivateLoadUnpackedFunction::FileSelected(
    const ui::SelectedFileInfo& file,
    int index) {
  select_file_dialog_.reset();
  StartFileLoad(file.path());
}

void DeveloperPrivateLoadUnpackedFunction::FileSelectionCanceled() {
  select_file_dialog_.reset();
  // This isn't really an error, but we should keep it like this for
  // backward compatibility.
  Finish(Error(kFileSelectionCanceled));
}

void DeveloperPrivateLoadUnpackedFunction::StartFileLoad(
    const base::FilePath file_path) {
  scoped_refptr<UnpackedInstaller> installer(
      UnpackedInstaller::Create(browser_context()));
  installer->set_be_noisy_on_failure(!fail_quietly_);
  installer->set_completion_callback(base::BindOnce(
      &DeveloperPrivateLoadUnpackedFunction::OnLoadComplete, this));
  installer->Load(file_path);

  retry_guid_ = DeveloperPrivateAPI::Get(browser_context())
                    ->AddUnpackedPath(GetSenderWebContents(), file_path);
}

void DeveloperPrivateLoadUnpackedFunction::OnLoadComplete(
    const Extension* extension,
    const base::FilePath& file_path,
    const std::string& error) {
  if (extension) {
    Finish(NoArguments());
    return;
  }

  if (!populate_error_) {
    Finish(Error(error));
    return;
  }

  GetManifestError(
      error, file_path,
      base::BindOnce(&DeveloperPrivateLoadUnpackedFunction::OnGotManifestError,
                     this));
}

void DeveloperPrivateLoadUnpackedFunction::OnGotManifestError(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest) {
  DCHECK(!retry_guid_.empty());
  Finish(WithArguments(
      CreateLoadError(file_path, error, line_number, manifest, retry_guid_)
          .ToValue()));
}

void DeveloperPrivateLoadUnpackedFunction::Finish(
    ResponseValue response_value) {
  Respond(std::move(response_value));
  Release();  // Balanced in Run().
}

void DeveloperPrivatePackDirectoryFunction::OnPackSuccess(
    const base::FilePath& crx_file,
    const base::FilePath& pem_file) {
  developer::PackDirectoryResponse response;
  response.message = base::UTF16ToUTF8(
      PackExtensionJob::StandardSuccessMessage(crx_file, pem_file));
  response.status = developer::PackStatus::kSuccess;
  Respond(WithArguments(response.ToValue()));
  pack_job_.reset();
  Release();  // Balanced in Run().
}

void DeveloperPrivatePackDirectoryFunction::OnPackFailure(
    const std::string& error,
    ExtensionCreator::ErrorType error_type) {
  developer::PackDirectoryResponse response;
  response.message = error;
  if (error_type == ExtensionCreator::kCRXExists) {
    response.item_path = item_path_str_;
    response.pem_path = key_path_str_;
    response.override_flags = ExtensionCreator::kOverwriteCRX;
    response.status = developer::PackStatus::kWarning;
  } else {
    response.status = developer::PackStatus::kError;
  }
  Respond(WithArguments(response.ToValue()));
  pack_job_.reset();
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction DeveloperPrivatePackDirectoryFunction::Run() {
  std::optional<PackDirectory::Params> params =
      PackDirectory::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int flags = params->flags ? *params->flags : 0;
  item_path_str_ = params->path;
  if (params->private_key_path) {
    key_path_str_ = *params->private_key_path;
  }

  base::FilePath root_directory =
      base::FilePath::FromUTF8Unsafe(item_path_str_);
  base::FilePath key_file = base::FilePath::FromUTF8Unsafe(key_path_str_);

  developer::PackDirectoryResponse response;
  if (root_directory.empty()) {
    if (item_path_str_.empty()) {
      response.message = l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_REQUIRED);
    } else {
      response.message = l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_INVALID);
    }

    response.status = developer::PackStatus::kError;
    return RespondNow(WithArguments(response.ToValue()));
  }

  if (!key_path_str_.empty() && key_file.empty()) {
    response.message =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PACK_DIALOG_ERROR_KEY_INVALID);
    response.status = developer::PackStatus::kError;
    return RespondNow(WithArguments(response.ToValue()));
  }

  AddRef();  // Balanced in OnPackSuccess / OnPackFailure.

  pack_job_ =
      std::make_unique<PackExtensionJob>(this, root_directory, key_file, flags);
  pack_job_->Start();
  return RespondLater();
}

DeveloperPrivatePackDirectoryFunction::DeveloperPrivatePackDirectoryFunction() =
    default;

DeveloperPrivatePackDirectoryFunction::
    ~DeveloperPrivatePackDirectoryFunction() = default;

ExtensionFunction::ResponseAction DeveloperPrivateLoadDirectoryFunction::Run() {
  // In theory `extension()` can be null when an ExtensionFunction is invoked
  // from WebUI, but this should never be the case for this particular API.
  DCHECK(extension());

  // TODO(grv) : add unittests.
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 3);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[2].is_string());

  const std::string& filesystem_name = args()[0].GetString();
  const std::string& filesystem_path = args()[1].GetString();
  const std::string& directory_url_str = args()[2].GetString();

  context_ = browser_context()
                 ->GetStoragePartition(render_frame_host()->GetSiteInstance())
                 ->GetFileSystemContext();

  // Directory url is non empty only for syncfilesystem.
  if (!directory_url_str.empty()) {
    storage::FileSystemURL directory_url =
        context_->CrackURLInFirstPartyContext(GURL(directory_url_str));
    if (!directory_url.is_valid() ||
        directory_url.type() != storage::kFileSystemTypeSyncable) {
      return RespondNow(Error("DirectoryEntry of unsupported filesystem."));
    }
    return LoadByFileSystemAPI(directory_url);
  }

  std::string unused_error;
  // Check if the DirectoryEntry is the instance of chrome filesystem.
  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(),
          &project_base_path_, &unused_error)) {
    return RespondNow(Error("DirectoryEntry of unsupported filesystem."));
  }

  // Try to load using the FileSystem API backend, in case the filesystem
  // points to a non-native local directory.
  std::string filesystem_id;
  bool cracked =
      storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id);
  CHECK(cracked);
  base::FilePath virtual_path =
      storage::IsolatedContext::GetInstance()
          ->CreateVirtualRootPath(filesystem_id)
          .Append(base::FilePath::FromUTF8Unsafe(filesystem_path));
  storage::FileSystemURL directory_url = context_->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFirstParty(extension()->origin()),
      storage::kFileSystemTypeIsolated, virtual_path);

  if (directory_url.is_valid() &&
      directory_url.type() != storage::kFileSystemTypeLocal &&
      directory_url.type() != storage::kFileSystemTypeDragged) {
    return LoadByFileSystemAPI(directory_url);
  }

  Load();
  return AlreadyResponded();
}

ExtensionFunction::ResponseAction
DeveloperPrivateLoadDirectoryFunction::LoadByFileSystemAPI(
    const storage::FileSystemURL& directory_url) {
  std::string directory_url_str = directory_url.ToGURL().spec();

  size_t pos = 0;
  // Parse the project directory name from the project url. The project url is
  // expected to have project name as the suffix.
  if ((pos = directory_url_str.rfind("/")) == std::string::npos) {
    return RespondNow(Error("Invalid Directory entry."));
  }

  std::string project_name;
  project_name = directory_url_str.substr(pos + 1);
  project_base_url_ = directory_url_str.substr(0, pos + 1);

  base::FilePath project_path(browser_context()->GetPath());
  project_path = project_path.AppendASCII(kUnpackedAppsFolder);
  project_path =
      project_path.Append(base::FilePath::FromUTF8Unsafe(project_name));

  project_base_path_ = project_path;

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &DeveloperPrivateLoadDirectoryFunction::ClearExistingDirectoryContent,
          this, project_base_path_));
  return RespondLater();
}

void DeveloperPrivateLoadDirectoryFunction::Load() {
  UnpackedInstaller::Create(browser_context())->Load(project_base_path_);

  // TODO(grv) : The unpacked installer should fire an event when complete
  // and return the extension_id.
  Respond(WithArguments("-1"));
}

void DeveloperPrivateLoadDirectoryFunction::ClearExistingDirectoryContent(
    const base::FilePath& project_path) {
  // Clear the project directory before copying new files.
  base::DeletePathRecursively(project_path);

  pending_copy_operations_count_ = 1;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPI,
          this, project_path, project_path.BaseName()));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPI(
    const base::FilePath& project_path,
    const base::FilePath& destination_path) {
  GURL project_url = GURL(project_base_url_ + destination_path.AsUTF8Unsafe());
  storage::FileSystemURL url =
      context_->CrackURLInFirstPartyContext(project_url);

  context_->operation_runner()->ReadDirectory(
      url, base::BindRepeating(&DeveloperPrivateLoadDirectoryFunction::
                                   ReadDirectoryByFileSystemAPICb,
                               this, project_path, destination_path));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPICb(
    const base::FilePath& project_path,
    const base::FilePath& destination_path,
    base::File::Error status,
    storage::FileSystemOperation::FileEntryList file_list,
    bool has_more) {
  if (status != base::File::FILE_OK) {
    DLOG(ERROR) << "Error in copying files from sync filesystem.";
    return;
  }

  // We add 1 to the pending copy operations for both files and directories. We
  // release the directory copy operation once all the files under the directory
  // are added for copying. We do that to ensure that pendingCopyOperationsCount
  // does not become zero before all copy operations are finished.
  // In case the directory happens to be executing the last copy operation it
  // will call Respond to send the response to the API. The pending copy
  // operations of files are released by the CopyFile function.
  pending_copy_operations_count_ += file_list.size();

  for (auto& file : file_list) {
    if (file.type == filesystem::mojom::FsFileType::DIRECTORY) {
      ReadDirectoryByFileSystemAPI(project_path.Append(file.name),
                                   destination_path.Append(file.name));
      continue;
    }

    GURL project_url = GURL(project_base_url_ +
                            destination_path.Append(file.name).AsUTF8Unsafe());
    storage::FileSystemURL url =
        context_->CrackURLInFirstPartyContext(project_url);

    base::FilePath target_path = project_path;
    target_path = target_path.Append(file.name);

    context_->operation_runner()->CreateSnapshotFile(
        url, base::BindOnce(
                 &DeveloperPrivateLoadDirectoryFunction::SnapshotFileCallback,
                 this, target_path));
  }

  if (!has_more) {
    // Directory copy operation released here.
    pending_copy_operations_count_--;

    if (!pending_copy_operations_count_) {
      ExtensionFunction::ResponseValue response =
          success_ ? NoArguments() : Error(error_);
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&DeveloperPrivateLoadDirectoryFunction::Respond, this,
                         std::move(response)));
    }
  }
}

void DeveloperPrivateLoadDirectoryFunction::SnapshotFileCallback(
    const base::FilePath& target_path,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& src_path,
    scoped_refptr<storage::ShareableFileReference> file_ref) {
  if (result != base::File::FILE_OK) {
    error_ = "Error in copying files from sync filesystem.";
    success_ = false;
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DeveloperPrivateLoadDirectoryFunction::CopyFile, this,
                     src_path, target_path));
}

void DeveloperPrivateLoadDirectoryFunction::CopyFile(
    const base::FilePath& src_path,
    const base::FilePath& target_path) {
  if (!base::CreateDirectory(target_path.DirName())) {
    error_ = "Error in copying files from sync filesystem.";
    success_ = false;
  }

  if (success_) {
    base::CopyFile(src_path, target_path);
  }

  CHECK(pending_copy_operations_count_ > 0);
  pending_copy_operations_count_--;

  if (!pending_copy_operations_count_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DeveloperPrivateLoadDirectoryFunction::Load, this));
  }
}

DeveloperPrivateLoadDirectoryFunction::DeveloperPrivateLoadDirectoryFunction()
    : pending_copy_operations_count_(0), success_(true) {}

DeveloperPrivateLoadDirectoryFunction::
    ~DeveloperPrivateLoadDirectoryFunction() {}

DeveloperPrivateShowOptionsFunction::~DeveloperPrivateShowOptionsFunction() =
    default;

ExtensionFunction::ResponseAction DeveloperPrivateShowOptionsFunction::Run() {
  std::optional<developer::ShowOptions::Params> params =
      developer::ShowOptions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetEnabledExtensionById(params->extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  if (OptionsPageInfo::GetOptionsPage(extension).is_empty()) {
    return RespondNow(Error(kNoOptionsPageForExtensionError));
  }

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

  ExtensionTabUtil::OpenOptionsPage(extension,
                                    chrome::FindBrowserWithTab(web_contents));
  return RespondNow(NoArguments());
}

DeveloperPrivateShowPathFunction::~DeveloperPrivateShowPathFunction() = default;

ExtensionFunction::ResponseAction DeveloperPrivateShowPathFunction::Run() {
  std::optional<developer::ShowPath::Params> params =
      developer::ShowPath::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  // We explicitly show manifest.json in order to work around an issue in OSX
  // where opening the directory doesn't focus the Finder.
  platform_util::ShowItemInFolder(
      Profile::FromBrowserContext(browser_context()),
      extension->path().Append(kManifestFilename));
  return RespondNow(NoArguments());
}

DeveloperPrivateSetShortcutHandlingSuspendedFunction::
    ~DeveloperPrivateSetShortcutHandlingSuspendedFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateSetShortcutHandlingSuspendedFunction::Run() {
  std::optional<developer::SetShortcutHandlingSuspended::Params> params =
      developer::SetShortcutHandlingSuspended::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ExtensionCommandsGlobalRegistry::Get(browser_context())
      ->SetShortcutHandlingSuspended(params->is_suspended);
  return RespondNow(NoArguments());
}

DeveloperPrivateRemoveMultipleExtensionsFunction::
    DeveloperPrivateRemoveMultipleExtensionsFunction() = default;
DeveloperPrivateRemoveMultipleExtensionsFunction::
    ~DeveloperPrivateRemoveMultipleExtensionsFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateRemoveMultipleExtensionsFunction::Run() {
  std::optional<developer::RemoveMultipleExtensions::Params> params =
      developer::RemoveMultipleExtensions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  profile_ = Profile::FromBrowserContext(browser_context());
  extension_ids_ = std::move(params->extension_ids);

  // Verify the input extension list.
  for (const auto& extension_id : extension_ids_) {
    CHECK(profile_);
    const Extension* current_extension =
        ExtensionRegistry::Get(profile_)->GetExtensionById(
            extension_id, ExtensionRegistry::EVERYTHING);
    if (!current_extension) {
      // Return early if the extension is a non-existent extension.
      return RespondNow(Error(kFailToUninstallNoneExistentExtensions));
    }
    // If enterprise or component extensions are found, do nothing and respond
    // with an error.
    if (Manifest::IsComponentLocation(current_extension->location()) ||
        Manifest::IsPolicyLocation(current_extension->location())) {
      return RespondNow(Error(kFailToUninstallEnterpriseOrComponentExtensions));
    }
  }

  if (accept_bubble_for_testing_.has_value()) {
    if (*accept_bubble_for_testing_) {
      OnDialogAccepted();
    } else {
      OnDialogCancelled();
    }
    return AlreadyResponded();
  }

  gfx::NativeWindow parent;
  if (!GetSenderWebContents()) {
    CHECK_IS_TEST();
    parent = gfx::NativeWindow();
  } else {
    parent = chrome::FindBrowserWithTab(GetSenderWebContents())
                 ->window()
                 ->GetNativeWindow();
  }

  ShowExtensionMultipleUninstallDialog(
      profile_, parent, extension_ids_,
      base::BindOnce(
          &DeveloperPrivateRemoveMultipleExtensionsFunction::OnDialogAccepted,
          this),
      base::BindOnce(
          &DeveloperPrivateRemoveMultipleExtensionsFunction::OnDialogCancelled,
          this));
  return RespondLater();
}

void DeveloperPrivateRemoveMultipleExtensionsFunction::OnDialogCancelled() {
  // Let the consumer end know that the Close button was clicked.
  Respond(Error(kUserCancelledError));
}

void DeveloperPrivateRemoveMultipleExtensionsFunction::OnDialogAccepted() {
  for (const auto& extension_id : extension_ids_) {
    if (!browser_context()) {
      return;
    }
    const Extension* current_extension =
        ExtensionRegistry::Get(profile_)->GetExtensionById(
            extension_id, ExtensionRegistry::EVERYTHING);
    // Extensions can be uninstalled externally while the dialog is open. Only
    // uninstall extensions that are still existent.
    if (!current_extension) {
      continue;
    }
    // If an extension fails to be uninstalled, it will not pause the
    // uninstall of the other extensions on the list.
    ExtensionRegistrar::Get(profile_)->UninstallExtension(
        extension_id, UNINSTALL_REASON_USER_INITIATED, nullptr);
  }
  Respond(NoArguments());
}

DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction() = default;
DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    ~DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction() =
        default;

ExtensionFunction::ResponseAction
DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::Run() {
  std::optional<developer::DismissMv2DeprecationNoticeForExtension::Params>
      params =
          developer::DismissMv2DeprecationNoticeForExtension::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(params);
  extension_id_ = std::move(params->extension_id);

  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());

  // Extension must be affected by the MV2 deprecation.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return RespondNow(Error(
        ErrorUtils::FormatErrorMessage(kNoExtensionError, extension_id_)));
  }
  if (!experiment_manager->IsExtensionAffected(*extension)) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        kExtensionNotAffectedByMV2Deprecation, extension_id_)));
  }

  MV2ExperimentStage experiment_stage =
      experiment_manager->GetCurrentExperimentStage();
  switch (experiment_stage) {
    case MV2ExperimentStage::kNone:
      NOTREACHED();

    case MV2ExperimentStage::kWarning: {
      // Immediately dismiss the notice.
      DismissExtensionNotice();
      return RespondNow(NoArguments());
    }

    case MV2ExperimentStage::kDisableWithReEnable: {
      // Prompt for user confirmation before dismissing the notice.
      if (accept_bubble_for_testing_.has_value()) {
        if (*accept_bubble_for_testing_) {
          OnDialogAccepted();
        } else {
          OnDialogCancelled();
        }
        return AlreadyResponded();
      }

      Browser* browser = chrome::FindLastActiveWithProfile(
          Profile::FromBrowserContext(browser_context()));
      if (!browser) {
        return RespondNow(Error(kCouldNotFindWebContentsError));
      }

      ShowMv2DeprecationKeepDialog(
          browser, *extension,
          base::BindOnce(
              &DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
                  OnDialogAccepted,
              this),
          base::BindOnce(
              &DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
                  OnDialogCancelled,
              this));

      return RespondLater();
    }

    case MV2ExperimentStage::kUnsupported:
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          kCannotDismissExtensionOnUnsupportedStage, extension_id_)));
  }
}

void DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    DismissExtensionNotice() {
  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());
  experiment_manager->MarkNoticeAsAcknowledged(extension_id_);

  // There isn't a separate observer for the MV2 acknowledged state changing,
  // but this is the only place it's changed. Just fire the event directly.
  DeveloperPrivateEventRouter* event_router =
      DeveloperPrivateAPI::Get(browser_context())
          ->developer_private_event_router();
  if (event_router) {
    event_router->OnExtensionConfigurationChanged(extension_id_);
  }
}

void DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    OnDialogAccepted() {
  if (!browser_context()) {
    return;
  }

  DismissExtensionNotice();
  Respond(NoArguments());
}

void DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    OnDialogCancelled() {
  if (!browser_context()) {
    return;
  }

  Respond(NoArguments());
}

DeveloperPrivateUploadExtensionToAccountFunction::
    DeveloperPrivateUploadExtensionToAccountFunction() = default;
DeveloperPrivateUploadExtensionToAccountFunction::
    ~DeveloperPrivateUploadExtensionToAccountFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateUploadExtensionToAccountFunction::Run() {
  auto params = developer::UploadExtensionToAccount::Params::Create(args());

  EXTENSION_FUNCTION_VALIDATE(params);
  extension_id_ = std::move(params->extension_id);
  profile_ = Profile::FromBrowserContext(browser_context());

  auto result = VerifyExtensionAndSigninState();
  if (!result.has_value()) {
    return RespondNow(Error(result.error()));
  }
  const Extension* extension = *result;

  // Return an error if the extension cannot be uploaded for reasons such as:
  // - syncing extensions in transport mode (signed in but not full sync) is
  //   disabled.
  // - the extension is already associated with the signed in user's account.
  // - the extension is not syncable (for example, if it's unpacked).
  if (!switches::IsExtensionsExplicitBrowserSigninEnabled() ||
      !AccountExtensionTracker::Get(profile_)->CanUploadAsAccountExtension(
          *extension)) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        kCannotUploadExtensionToAccount, extension_id_)));
  }

  if (accept_bubble_for_testing_.has_value()) {
    if (*accept_bubble_for_testing_) {
      OnDialogAccepted();
    } else {
      OnDialogCancelled();
    }
    return AlreadyResponded();
  }

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

  ShowUploadExtensionToAccountDialog(
      browser, *extension,
      base::BindOnce(
          &DeveloperPrivateUploadExtensionToAccountFunction::OnDialogAccepted,
          this),
      base::BindOnce(
          &DeveloperPrivateUploadExtensionToAccountFunction::OnDialogCancelled,
          this));

  return RespondLater();
}

base::expected<const Extension*, std::string>
DeveloperPrivateUploadExtensionToAccountFunction::
    VerifyExtensionAndSigninState() {
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return base::unexpected(
        ErrorUtils::FormatErrorMessage(kNoExtensionError, extension_id_));
  }

  // Return an error if there is no signed in user.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  if (account_info.IsEmpty()) {
    return base::unexpected(kUserNotSignedIn);
  }

  return base::ok(extension);
}

void DeveloperPrivateUploadExtensionToAccountFunction::UploadExtensionToAccount(
    const Extension& extension) {
  AccountExtensionTracker::Get(browser_context())
      ->OnAccountUploadInitiatedForExtension(extension.id());
  ExtensionSyncService::Get(browser_context())
      ->SyncExtensionChangeIfNeeded(extension);
}

void DeveloperPrivateUploadExtensionToAccountFunction::OnDialogAccepted() {
  // We cannot proceed if the `browser_context` is not valid as the relevant
  // classes needed to upload the extension will not exist.
  if (!browser_context()) {
    return;
  }

  auto result = VerifyExtensionAndSigninState();
  if (!result.has_value()) {
    Respond(Error(result.error()));
    return;
  }
  const Extension* extension = *result;

  UploadExtensionToAccount(*extension);
  Respond(WithArguments(true));
}

void DeveloperPrivateUploadExtensionToAccountFunction::OnDialogCancelled() {
  Respond(WithArguments(false));
}

}  // namespace api

}  // namespace extensions
