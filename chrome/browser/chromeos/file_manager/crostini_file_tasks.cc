// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/crostini_file_tasks.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/layout.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

namespace {

constexpr base::TimeDelta kIconLoadTimeout =
    base::TimeDelta::FromMilliseconds(100);
constexpr size_t kIconSizeInDip = 16;
// When MIME type detection is done; if we can't be properly determined then
// the detection system will end up guessing one of the 2 values below depending
// upon whether it "thinks" it is binary or text content.
constexpr char kUnknownBinaryMimeType[] = "application/octet-stream";
constexpr char kUnknownTextMimeType[] = "text/plain";

GURL GeneratePNGDataUrl(const SkBitmap& sk_bitmap) {
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(sk_bitmap, false /* discard_transparency */,
                                    &output);
  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(output.data()),
                        output.size()),
      &encoded);
  return GURL("data:image/png;base64," + encoded);
}

void OnAppIconsLoaded(Profile* profile,
                      const std::vector<std::string>& app_ids,
                      const std::vector<std::string>& app_names,
                      ui::ScaleFactor scale_factor,
                      std::vector<FullTaskDescriptor>* result_list,
                      base::OnceClosure completion_closure,
                      const std::vector<gfx::ImageSkia>& icons) {
  DCHECK(!app_ids.empty());
  DCHECK_EQ(app_ids.size(), icons.size());

  float scale = ui::GetScaleForScaleFactor(scale_factor);

  for (size_t i = 0; i < app_ids.size(); ++i) {
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(app_ids[i], TASK_TYPE_CROSTINI_APP,
                       kCrostiniAppActionID),
        app_names[i],
        extensions::api::file_manager_private::Verb::VERB_OPEN_WITH,
        GeneratePNGDataUrl(icons[i].GetRepresentation(scale).GetBitmap()),
        false /* is_default */, false /* is_generic */,
        false /* is_file_extension_match */));
  }

  std::move(completion_closure).Run();
}

void OnTaskComplete(FileTaskFinishedCallback done,
                    bool success,
                    const std::string& failure_reason) {
  if (!success) {
    LOG(ERROR) << "Crostini task error: " << failure_reason;
  }
  std::move(done).Run(
      success ? extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT
              : extensions::api::file_manager_private::TASK_RESULT_FAILED,
      failure_reason);
}

}  // namespace

void FindCrostiniTasks(Profile* profile,
                       const std::vector<extensions::EntryInfo>& entries,
                       std::vector<FullTaskDescriptor>* result_list,
                       base::OnceClosure completion_closure) {
  if (!crostini::CrostiniFeatures::Get()->IsUIAllowed(profile)) {
    std::move(completion_closure).Run();
    return;
  }

  std::vector<std::string> result_app_ids;
  std::vector<std::string> result_app_names;

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  crostini::CrostiniMimeTypesService* mime_types_service =
      crostini::CrostiniMimeTypesServiceFactory::GetForProfile(profile);
  for (const auto& pair : registry_service->GetRegisteredApps()) {
    const std::string& app_id = pair.first;
    const auto& registration = pair.second;
    const std::set<std::string>& supported_mime_types =
        registration.MimeTypes();
    bool had_unsupported_mime_type = false;
    for (const extensions::EntryInfo& entry : entries) {
      if (supported_mime_types.find(entry.mime_type) !=
          supported_mime_types.end())
        continue;
      // If we see either of these then we use the Linux container MIME type
      // mappings as alternates for finding an appropriate app since these are
      // the defaults when Chrome can't figure out the exact MIME type (but they
      // can also be the actual MIME type, so we don't exclude them above).
      if (entry.mime_type == kUnknownBinaryMimeType ||
          entry.mime_type == kUnknownTextMimeType) {
        std::string alternate_mime_type = mime_types_service->GetMimeType(
            entry.path, registration.VmName(), registration.ContainerName());
        if (supported_mime_types.find(alternate_mime_type) !=
            supported_mime_types.end())
          continue;
      }
      had_unsupported_mime_type = true;
      break;
    }
    if (had_unsupported_mime_type)
      continue;
    result_app_ids.push_back(app_id);
    result_app_names.push_back(registration.Name());
  }

  if (result_app_ids.empty()) {
    std::move(completion_closure).Run();
    return;
  }

  ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors().back();

  crostini::LoadIcons(
      profile, result_app_ids, kIconSizeInDip, scale_factor, kIconLoadTimeout,
      base::BindOnce(OnAppIconsLoaded, profile, result_app_ids,
                     result_app_names, scale_factor, result_list,
                     std::move(completion_closure)));
}

void ExecuteCrostiniTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    FileTaskFinishedCallback done) {
  DCHECK(crostini::CrostiniFeatures::Get()->IsUIAllowed(profile));

  crostini::LaunchCrostiniApp(profile, task.app_id, display::kInvalidDisplayId,
                              file_system_urls,
                              base::BindOnce(OnTaskComplete, std::move(done)));
}

}  // namespace file_tasks
}  // namespace file_manager
