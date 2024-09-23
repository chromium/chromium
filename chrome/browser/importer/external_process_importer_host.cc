// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/external_process_importer_host.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/importer/external_process_importer_client.h"
#include "chrome/browser/importer/firefox_profile_lock.h"
#include "chrome/browser/importer/importer_lock_dialog.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/in_process_importer_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"

using bookmarks::BookmarkModel;
using content::BrowserThread;

ExternalProcessImporterHost::ExternalProcessImporterHost()
    : headless_(false),
      parent_window_(nullptr),
      observer_(nullptr),
      profile_(nullptr),
      is_source_readable_(true),
      client_(nullptr),
      items_(0),
      cancelled_(false) {}

void ExternalProcessImporterHost::Cancel() {
  cancelled_ = true;
  // There is only a |client_| if the import was started.
  if (client_)
    client_->Cancel();
  NotifyImportEnded();  // Tells the observer that we're done, and deletes us.
}

void ExternalProcessImporterHost::StartImportSettings(
    const importer::SourceProfile& source_profile,
    Profile* target_profile,
    uint16_t items,
    ProfileWriter* writer) {
  // We really only support importing from one host at a time.
  DCHECK(!profile_);
  DCHECK(target_profile);

  profile_ = target_profile;
  writer_ = writer;
  source_profile_ = source_profile;
  items_ = items;

  if (!CheckForFirefoxLock(source_profile)) {
    Cancel();
    return;
  }

  CheckForLoadedModels(items);

  LaunchImportIfReady();
}

void ExternalProcessImporterHost::NotifyImportStarted() {
  if (observer_)
    observer_->ImportStarted();
}

void ExternalProcessImporterHost::NotifyImportItemStarted(
    importer::ImportItem item) {
  if (observer_)
    observer_->ImportItemStarted(item);
}

void ExternalProcessImporterHost::NotifyImportItemEnded(
    importer::ImportItem item) {
  if (observer_)
    observer_->ImportItemEnded(item);
}

void ExternalProcessImporterHost::NotifyImportEnded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  firefox_lock_.reset();
  if (observer_)
    observer_->ImportEnded();
  delete this;
}

ExternalProcessImporterHost::~ExternalProcessImporterHost() = default;

void ExternalProcessImporterHost::LaunchImportIfReady() {
  if (bookmark_model_observation_for_loading_.IsObserving() ||
      template_service_subscription_ || !is_source_readable_ || cancelled_) {
    return;
  }

  // This is the in-process half of the bridge, which catches data from the IPC
  // pipe and feeds it to the ProfileWriter. The external process half of the
  // bridge lives in the external process (see ProfileImportThread).
  // The ExternalProcessImporterClient created in the next line owns the bridge,
  // and will delete it.
  InProcessImporterBridge* bridge =
      new InProcessImporterBridge(writer_.get(),
                                  weak_ptr_factory_.GetWeakPtr());
  client_ = new ExternalProcessImporterClient(
      weak_ptr_factory_.GetWeakPtr(), source_profile_, items_, bridge);
  client_->Start();
}

void ExternalProcessImporterHost::BookmarkModelLoaded(bool ids_reassigned) {
  bookmark_model_observation_for_loading_.Reset();

  LaunchImportIfReady();
}

void ExternalProcessImporterHost::BookmarkModelBeingDeleted() {
  bookmark_model_observation_for_loading_.Reset();
}

void ExternalProcessImporterHost::BookmarkModelChanged() {
}

void ExternalProcessImporterHost::OnTemplateURLServiceLoaded() {
  template_service_subscription_ = {};
  LaunchImportIfReady();
}

void ExternalProcessImporterHost::ShowWarningDialog() {
  DCHECK(!headless_);
  importer::ShowImportLockDialog(
      parent_window_,
      base::BindOnce(&ExternalProcessImporterHost::OnImportLockDialogEnd,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExternalProcessImporterHost::OnImportLockDialogEnd(bool is_continue) {
  if (is_continue) {
    // User chose to continue, then we check the lock again to make
    // sure that Firefox has been closed. Try to import the settings
    // if successful. Otherwise, show a warning dialog.
    firefox_lock_->Lock();
    if (firefox_lock_->HasAcquired()) {
      is_source_readable_ = true;
      LaunchImportIfReady();
    } else {
      ShowWarningDialog();
    }
  } else {
    NotifyImportEnded();
  }
}

bool ExternalProcessImporterHost::CheckForFirefoxLock(
    const importer::SourceProfile& source_profile) {
  if (source_profile.importer_type != importer::TYPE_FIREFOX)
    return true;

  DCHECK(!firefox_lock_.get());
  firefox_lock_ =
      std::make_unique<FirefoxProfileLock>(source_profile.source_path);
  if (firefox_lock_->HasAcquired())
    return true;

  // If fail to acquire the lock, we set the source unreadable and
  // show a warning dialog, unless running without UI (in which case the import
  // must be aborted).
  is_source_readable_ = false;
  if (headless_)
    return false;

  ShowWarningDialog();
  return true;
}

void ExternalProcessImporterHost::CheckForLoadedModels(uint16_t items) {
  // A target profile must be loaded by StartImportSettings().
  DCHECK(profile_);

  // BookmarkModel should be loaded before adding IE favorites. So we observe
  // the BookmarkModel if needed, and start the task after it has been loaded.
  if ((items & importer::FAVORITES) && !writer_->BookmarkModelIsLoaded()) {
    bookmark_model_observation_for_loading_.Observe(
        BookmarkModelFactory::GetForBrowserContext(profile_));
  }

  // Observes the TemplateURLService if needed to import search engines from the
  // other browser. We also check to see if we're importing bookmarks because
  // we can import bookmark keywords from Firefox as search engines.
  if ((items & importer::SEARCH_ENGINES) || (items & importer::FAVORITES)) {
    if (!writer_->TemplateURLServiceIsLoaded()) {
      TemplateURLService* model =
          TemplateURLServiceFactory::GetForProfile(profile_);
      template_service_subscription_ =
          model->RegisterOnLoadedCallback(base::BindOnce(
              &ExternalProcessImporterHost::OnTemplateURLServiceLoaded,
              weak_ptr_factory_.GetWeakPtr()));
      model->Load();
    }
  }
}
