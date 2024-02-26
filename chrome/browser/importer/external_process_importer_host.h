// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_HOST_H_
#define CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/common/importer/importer_data_types.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/search_engines/template_url_service.h"
#include "ui/gfx/native_widget_types.h"

class ExternalProcessImporterClient;
class FirefoxProfileLock;
class Profile;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace importer {
struct SourceProfile;
}  // namespace importer

// This class manages the import process. It creates the in-process half of the
// importer bridge and the external process importer client.
class ExternalProcessImporterHost
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  ExternalProcessImporterHost();

  ExternalProcessImporterHost(const ExternalProcessImporterHost&) = delete;
  ExternalProcessImporterHost& operator=(const ExternalProcessImporterHost&) =
      delete;

  void Cancel();

  // Starts the process of importing the settings and data depending on what the
  // user selected.
  // |source_profile| - importer profile to import.
  // |target_profile| - profile to import into.
  // |items| - specifies which data to import (bitmask of importer::ImportItem).
  // |writer| - called to actually write data back to the profile.
  virtual void StartImportSettings(
      const importer::SourceProfile& source_profile,
      Profile* target_profile,
      uint16_t items,
      ProfileWriter* writer);

  // When in headless mode, the importer will not show any warning dialog if
  // a user action is required (e.g., Firefox profile is locked and user should
  // close Firefox to continue) and the outcome is as if the user had canceled
  // the import operation.
  void set_headless() { headless_ = true; }
  bool is_headless() const { return headless_; }

  void set_parent_window(gfx::NativeWindow parent_window) {
    parent_window_ = parent_window;
  }

  void set_observer(importer::ImporterProgressObserver* observer) {
    observer_ = observer;
  }

  // A series of functions invoked at the start, during and end of the import
  // process. The middle functions are notifications that the a harvesting of a
  // particular source of data (specified by |item|) is under way.
  void NotifyImportStarted();
  void NotifyImportItemStarted(importer::ImportItem item);
  void NotifyImportItemEnded(importer::ImportItem item);
  void NotifyImportEnded();

 private:
  // ExternalProcessImporterHost deletes itself in OnImportEnded().
  ~ExternalProcessImporterHost() override;

  // Launches the utility process that starts the import task, unless bookmark
  // or template model are not yet loaded. If load is not detected, this method
  // will be called when the loading observer sees that model loading is
  // complete.
  virtual void LaunchImportIfReady();

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkModelChanged() override;

  // Called when TemplateURLService has been loaded.
  void OnTemplateURLServiceLoaded();

  // ShowWarningDialog() asks user to close the application that is owning the
  // lock. They can retry or skip the importing process.
  // This method should not be called if the importer is in headless mode.
  void ShowWarningDialog();

  // This is called when when user ends the lock dialog by clicking on either
  // the "Skip" or "Continue" buttons. |is_continue| is true when user clicked
  // the "Continue" button.
  void OnImportLockDialogEnd(bool is_continue);

  // Make sure that Firefox isn't running, if import browser is Firefox. Show
  // to the user a dialog that notifies that is necessary to close Firefox
  // prior to continue.
  // |source_profile| - importer profile to import.
  // Returns false iff import should be aborted.
  bool CheckForFirefoxLock(const importer::SourceProfile& source_profile);

  // Make sure BookmarkModel and TemplateURLService are loaded before import
  // process starts, if bookmarks and/or search engines are among the items
  // which are to be imported.
  void CheckForLoadedModels(uint16_t items);

  // True if UI is not to be shown.
  bool headless_;

  // Parent window that we pass to the import lock dialog (i.e, the Firefox
  // warning dialog).
  gfx::NativeWindow parent_window_;

  // The observer that we need to notify about changes in the import process.
  raw_ptr<importer::ImporterProgressObserver, DanglingUntriaged> observer_;

  // Firefox profile lock.
  std::unique_ptr<FirefoxProfileLock> firefox_lock_;

  // Profile we're importing from.
  raw_ptr<Profile> profile_;

  // Set if we're waiting for the model to finish loading, and represents
  // the BookmarkModel instance we are waiting for.
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      bookmark_model_observation_for_loading_{this};

  // Non-empty when waiting for the TemplateURLService to finish loading.
  base::CallbackListSubscription template_service_subscription_;

  // True if source profile is readable.
  bool is_source_readable_;

  // Writes data from the importer back to the profile.
  scoped_refptr<ProfileWriter> writer_;

  // Used to pass notifications from the browser side to the external process.
  raw_ptr<ExternalProcessImporterClient, DanglingUntriaged> client_;

  // Information about a profile needed for importing.
  importer::SourceProfile source_profile_;

  // Bitmask of items to be imported (see importer::ImportItem enum).
  uint16_t items_;

  // True if the import process has been cancelled.
  bool cancelled_;

  // Vends weak pointers for the importer to call us back.
  base::WeakPtrFactory<ExternalProcessImporterHost> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_HOST_H_
