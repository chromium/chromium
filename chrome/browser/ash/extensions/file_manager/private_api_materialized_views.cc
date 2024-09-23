// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_materialized_views.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/files/file_error_or.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace extensions {

FileManagerPrivateGetMaterializedViewsFunction::
    FileManagerPrivateGetMaterializedViewsFunction() = default;

FileManagerPrivateGetMaterializedViewsFunction::
    ~FileManagerPrivateGetMaterializedViewsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetMaterializedViewsFunction::Run() {
  if (!base::FeatureList::IsEnabled(ash::features::kFilesMaterializedViews)) {
    return RespondNow(Error("Not enabled"));
  }

  std::vector<api::file_manager_private::MaterializedView> views;

  // TODO(b/327532840): Replace with proper implementation.
  ash::HoldingSpaceKeyedService* const holding_space =
      ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          browser_context());
  if (holding_space) {
    api::file_manager_private::MaterializedView view;
    view.view_id = 1;
    // TODO(b/327532840): Return string name translated.
    view.name = "Starred files";
    views.push_back(std::move(view));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetMaterializedViews::Results::Create(views)));
}

FileManagerPrivateReadMaterializedViewFunction::
    FileManagerPrivateReadMaterializedViewFunction() = default;

FileManagerPrivateReadMaterializedViewFunction::
    ~FileManagerPrivateReadMaterializedViewFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateReadMaterializedViewFunction::Run() {
  using extensions::api::file_manager_private::ReadMaterializedView::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!base::FeatureList::IsEnabled(ash::features::kFilesMaterializedViews)) {
    return RespondNow(Error("Not enabled"));
  }

  if (params->view_id != 1) {
    return RespondNow(Error("Unknown view"));
  }

  ash::HoldingSpaceKeyedService* const holding_space =
      ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          browser_context());
  if (!holding_space) {
    return RespondNow(Error("Holding space not enabled"));
  }

  std::vector<GURL> items = holding_space->GetPinnedFiles();

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  auto barrier_callback = base::BarrierCallback<
      base::FileErrorOr<api::file_manager_private::EntryData>>(
      items.size(),
      base::BindOnce(
          &FileManagerPrivateReadMaterializedViewFunction::OnEntryDataRetrieved,
          this));

  for (const GURL& item : items) {
    file_manager::util::GURLToEntryData(profile, file_system_context, item,
                                        barrier_callback);
  }

  return RespondLater();
}

void FileManagerPrivateReadMaterializedViewFunction::OnEntryDataRetrieved(
    std::vector<base::FileErrorOr<api::file_manager_private::EntryData>>
        entry_results) {
  std::vector<api::file_manager_private::EntryData> entries;
  for (auto& entry : entry_results) {
    if (entry.has_value()) {
      entries.push_back(std::move(*entry));
    }
  }
  Respond(ArgumentList(
      api::file_manager_private::ReadMaterializedView::Results::Create(
          entries)));
}

}  // namespace extensions
