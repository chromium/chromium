// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/common/extensions/api/downloads.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "content/public/browser/download_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/warning_set.h"

class DownloadFileIconExtractor;
class DownloadOpenPrompt;
class Profile;

// Functions in the chrome.downloads namespace facilitate
// controlling downloads from extensions. See the full API doc at
// http://goo.gl/6hO1n

namespace download_extension_errors {

// Errors that can be returned through chrome.runtime.lastError.message.
extern const char kEmptyFile[];
extern const char kFileAlreadyDeleted[];
extern const char kFileNotRemoved[];
extern const char kIconNotFound[];
extern const char kInvalidDangerType[];
extern const char kInvalidFilename[];
extern const char kInvalidFilter[];
extern const char kInvalidHeaderName[];
extern const char kInvalidHeaderValue[];
extern const char kInvalidHeaderUnsafe[];
extern const char kInvalidId[];
extern const char kInvalidOrderBy[];
extern const char kInvalidQueryLimit[];
extern const char kInvalidState[];
extern const char kInvalidURL[];
extern const char kInvisibleContext[];
extern const char kNotComplete[];
extern const char kNotDangerous[];
extern const char kNotInProgress[];
extern const char kNotResumable[];
extern const char kOpenPermission[];
extern const char kShelfDisabled[];
extern const char kShelfPermission[];
extern const char kTooManyListeners[];
extern const char kUiDisabled[];
extern const char kUiPermission[];
extern const char kUnexpectedDeterminer[];
extern const char kUserGesture[];

}  // namespace download_extension_errors

namespace extensions {

class DownloadedByExtension : public base::SupportsUserData::Data {
 public:
  static DownloadedByExtension* Get(download::DownloadItem* item);

  DownloadedByExtension(download::DownloadItem* item,
                        const std::string& id,
                        const std::string& name);

  DownloadedByExtension(const DownloadedByExtension&) = delete;
  DownloadedByExtension& operator=(const DownloadedByExtension&) = delete;

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }

 private:
  static const char kKey[];

  std::string id_;
  std::string name_;
};

class DownloadsDownloadFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.download", DOWNLOADS_DOWNLOAD)
  DownloadsDownloadFunction();

  DownloadsDownloadFunction(const DownloadsDownloadFunction&) = delete;
  DownloadsDownloadFunction& operator=(const DownloadsDownloadFunction&) =
      delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsDownloadFunction() override;

 private:
  void OnStarted(const base::FilePath& creator_suggested_filename,
                 extensions::api::downloads::FilenameConflictAction
                     creator_conflict_action,
                 download::DownloadItem* item,
                 download::DownloadInterruptReason interrupt_reason);
};

class DownloadsSearchFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.search", DOWNLOADS_SEARCH)
  DownloadsSearchFunction();

  DownloadsSearchFunction(const DownloadsSearchFunction&) = delete;
  DownloadsSearchFunction& operator=(const DownloadsSearchFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsSearchFunction() override;
};

class DownloadsPauseFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.pause", DOWNLOADS_PAUSE)
  DownloadsPauseFunction();

  DownloadsPauseFunction(const DownloadsPauseFunction&) = delete;
  DownloadsPauseFunction& operator=(const DownloadsPauseFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsPauseFunction() override;
};

class DownloadsResumeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.resume", DOWNLOADS_RESUME)
  DownloadsResumeFunction();

  DownloadsResumeFunction(const DownloadsResumeFunction&) = delete;
  DownloadsResumeFunction& operator=(const DownloadsResumeFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsResumeFunction() override;
};

class DownloadsCancelFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.cancel", DOWNLOADS_CANCEL)
  DownloadsCancelFunction();

  DownloadsCancelFunction(const DownloadsCancelFunction&) = delete;
  DownloadsCancelFunction& operator=(const DownloadsCancelFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsCancelFunction() override;
};

class DownloadsEraseFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.erase", DOWNLOADS_ERASE)
  DownloadsEraseFunction();

  DownloadsEraseFunction(const DownloadsEraseFunction&) = delete;
  DownloadsEraseFunction& operator=(const DownloadsEraseFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsEraseFunction() override;
};

class DownloadsRemoveFileFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.removeFile", DOWNLOADS_REMOVEFILE)
  DownloadsRemoveFileFunction();

  DownloadsRemoveFileFunction(const DownloadsRemoveFileFunction&) = delete;
  DownloadsRemoveFileFunction& operator=(const DownloadsRemoveFileFunction&) =
      delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsRemoveFileFunction() override;

 private:
  void Done(bool success);
};

class DownloadsAcceptDangerFunction : public ExtensionFunction {
 public:
  using OnPromptCreatedCallback =
      base::OnceCallback<void(DownloadDangerPrompt*)>;
  static void OnPromptCreatedForTesting(
      OnPromptCreatedCallback* callback) {
    on_prompt_created_ = callback;
  }

  DECLARE_EXTENSION_FUNCTION("downloads.acceptDanger", DOWNLOADS_ACCEPTDANGER)
  DownloadsAcceptDangerFunction();

  DownloadsAcceptDangerFunction(const DownloadsAcceptDangerFunction&) = delete;
  DownloadsAcceptDangerFunction& operator=(
      const DownloadsAcceptDangerFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsAcceptDangerFunction() override;
  void DangerPromptCallback(int download_id,
                            DownloadDangerPrompt::Action action);

 private:
  void PromptOrWait(int download_id, int retries);

  static OnPromptCreatedCallback* on_prompt_created_;
};

class DownloadsShowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.show", DOWNLOADS_SHOW)
  DownloadsShowFunction();

  DownloadsShowFunction(const DownloadsShowFunction&) = delete;
  DownloadsShowFunction& operator=(const DownloadsShowFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsShowFunction() override;
};

class DownloadsShowDefaultFolderFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "downloads.showDefaultFolder", DOWNLOADS_SHOWDEFAULTFOLDER)
  DownloadsShowDefaultFolderFunction();

  DownloadsShowDefaultFolderFunction(
      const DownloadsShowDefaultFolderFunction&) = delete;
  DownloadsShowDefaultFolderFunction& operator=(
      const DownloadsShowDefaultFolderFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsShowDefaultFolderFunction() override;
};

class DownloadsOpenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.open", DOWNLOADS_OPEN)
  DownloadsOpenFunction();

  DownloadsOpenFunction(const DownloadsOpenFunction&) = delete;
  DownloadsOpenFunction& operator=(const DownloadsOpenFunction&) = delete;

  ResponseAction Run() override;

  using OnPromptCreatedCallback = base::OnceCallback<void(DownloadOpenPrompt*)>;
  static void set_on_prompt_created_cb_for_testing(
      OnPromptCreatedCallback* on_prompt_created_cb) {
    on_prompt_created_cb_ = on_prompt_created_cb;
  }

 protected:
  ~DownloadsOpenFunction() override;

 private:
  void OpenPromptDone(int download_id, bool accept);

  static OnPromptCreatedCallback* on_prompt_created_cb_;
};

class DownloadsSetShelfEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.setShelfEnabled",
                             DOWNLOADS_SETSHELFENABLED)
  DownloadsSetShelfEnabledFunction();

  DownloadsSetShelfEnabledFunction(const DownloadsSetShelfEnabledFunction&) =
      delete;
  DownloadsSetShelfEnabledFunction& operator=(
      const DownloadsSetShelfEnabledFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsSetShelfEnabledFunction() override;
};

class DownloadsSetUiOptionsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.setUiOptions", DOWNLOADS_SETUIOPTIONS)
  DownloadsSetUiOptionsFunction();

  DownloadsSetUiOptionsFunction(const DownloadsSetUiOptionsFunction&) = delete;
  DownloadsSetUiOptionsFunction& operator=(
      const DownloadsSetUiOptionsFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~DownloadsSetUiOptionsFunction() override;
};

class DownloadsGetFileIconFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.getFileIcon", DOWNLOADS_GETFILEICON)
  DownloadsGetFileIconFunction();

  DownloadsGetFileIconFunction(const DownloadsGetFileIconFunction&) = delete;
  DownloadsGetFileIconFunction& operator=(const DownloadsGetFileIconFunction&) =
      delete;

  ResponseAction Run() override;
  void SetIconExtractorForTesting(DownloadFileIconExtractor* extractor);

 protected:
  ~DownloadsGetFileIconFunction() override;

 private:
  void OnIconURLExtracted(const std::string& url);
  base::FilePath path_;
  std::unique_ptr<DownloadFileIconExtractor> icon_extractor_;
};

// Observes a single DownloadManager and many DownloadItems and dispatches
// onCreated and onErased events.
class ExtensionDownloadsEventRouter
    : public extensions::EventRouter::Observer,
      public extensions::ExtensionRegistryObserver,
      public download::AllDownloadItemNotifier::Observer {
 public:
  typedef base::OnceCallback<void(
      const base::FilePath& changed_filename,
      download::DownloadPathReservationTracker::FilenameConflictAction)>
      FilenameChangedCallback;

  static void SetDetermineFilenameTimeoutSecondsForTesting(int s);

  // The logic for how to handle conflicting filename suggestions from multiple
  // extensions is split out here for testing.
  static void DetermineFilenameInternal(
      const base::FilePath& filename,
      extensions::api::downloads::FilenameConflictAction conflict_action,
      const std::string& suggesting_extension_id,
      const base::Time& suggesting_install_time,
      const std::string& incumbent_extension_id,
      const base::Time& incumbent_install_time,
      std::string* winner_extension_id,
      base::FilePath* determined_filename,
      extensions::api::downloads::FilenameConflictAction*
        determined_conflict_action,
      extensions::WarningSet* warnings);

  // A downloads.onDeterminingFilename listener has returned. If the extension
  // wishes to override the download's filename, then |filename| will be
  // non-empty. |filename| will be interpreted as a relative path, appended to
  // the default downloads directory. If the extension wishes to overwrite any
  // existing files, then |overwrite| will be true. Returns true on success,
  // false otherwise.
  static bool DetermineFilename(
      content::BrowserContext* browser_context,
      bool include_incognito,
      const std::string& ext_id,
      int download_id,
      const base::FilePath& filename,
      extensions::api::downloads::FilenameConflictAction conflict_action,
      std::string* error);

  explicit ExtensionDownloadsEventRouter(
      Profile* profile, content::DownloadManager* manager);

  ExtensionDownloadsEventRouter(const ExtensionDownloadsEventRouter&) = delete;
  ExtensionDownloadsEventRouter& operator=(
      const ExtensionDownloadsEventRouter&) = delete;

  ~ExtensionDownloadsEventRouter() override;

  void SetUiEnabled(const extensions::Extension* extension, bool enabled);
  bool IsUiEnabled() const;

  // Called by ChromeDownloadManagerDelegate during the filename determination
  // process, allows extensions to change the item's target filename. If no
  // extension wants to change the target filename, then |filename_changed| will
  // be called with an empty filename and the filename determination process
  // will continue as normal. If an extension wants to change the target
  // filename, then |filename_changed| will be called with the new filename and
  // a flag indicating whether the new file should overwrite any old files of
  // the same name.
  void OnDeterminingFilename(download::DownloadItem* item,
                             const base::FilePath& suggested_path,
                             FilenameChangedCallback filename_changed);

  // AllDownloadItemNotifier::Observer.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* download_item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* download_item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         download::DownloadItem* download_item) override;

  // extensions::EventRouter::Observer.
  void OnListenerRemoved(const extensions::EventListenerInfo& details) override;

  void CheckForHistoryFilesRemoval();

 private:
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     bool include_incognito,
                     Event::WillDispatchCallback will_dispatch_callback,
                     base::Value json_arg);

  // extensions::ExtensionRegistryObserver.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  raw_ptr<Profile> profile_;
  download::AllDownloadItemNotifier notifier_;
  std::set<raw_ptr<const extensions::Extension, SetExperimental>>
      ui_disabling_extensions_;

  base::Time last_checked_removal_;

  // Listen to extension unloaded notifications.
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_
