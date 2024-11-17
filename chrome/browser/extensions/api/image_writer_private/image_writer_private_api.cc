// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_private_api.h"

#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/disks/disks_prefs.h"
#endif

namespace image_writer_api = extensions::api::image_writer_private;

namespace extensions {

namespace {

}  // namespace

ImageWriterPrivateBaseFunction::ImageWriterPrivateBaseFunction() = default;

ImageWriterPrivateBaseFunction::~ImageWriterPrivateBaseFunction() = default;

void ImageWriterPrivateBaseFunction::OnComplete(bool success,
                                                const std::string& error) {
  if (success)
    Respond(NoArguments());
  else
    Respond(Error(error));
}

ImageWriterPrivateWriteFromUrlFunction::
    ImageWriterPrivateWriteFromUrlFunction() = default;

ImageWriterPrivateWriteFromUrlFunction::
    ~ImageWriterPrivateWriteFromUrlFunction() = default;

ExtensionFunction::ResponseAction
ImageWriterPrivateWriteFromUrlFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->GetPrefs()->GetBoolean(disks::prefs::kExternalStorageDisabled) ||
      profile->GetPrefs()->GetBoolean(disks::prefs::kExternalStorageReadOnly)) {
    return RespondNow(Error(image_writer::error::kDeviceWriteError));
  }
#endif
  std::optional<image_writer_api::WriteFromUrl::Params> params =
      image_writer_api::WriteFromUrl::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url(params->image_url);
  if (!url.is_valid())
    return RespondNow(Error(image_writer::error::kUrlInvalid));

  std::string hash;
  if (params->options && params->options->image_hash) {
    hash = *params->options->image_hash;
  }

  image_writer::OperationManager::Get(browser_context())
      ->StartWriteFromUrl(
          extension_id(), url, hash, params->storage_unit_id,
          base::BindOnce(&ImageWriterPrivateWriteFromUrlFunction::OnComplete,
                         this));

  return RespondLater();
}

ImageWriterPrivateWriteFromFileFunction::
    ImageWriterPrivateWriteFromFileFunction() = default;

ImageWriterPrivateWriteFromFileFunction::
    ~ImageWriterPrivateWriteFromFileFunction() = default;

ExtensionFunction::ResponseAction
ImageWriterPrivateWriteFromFileFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->GetPrefs()->GetBoolean(disks::prefs::kExternalStorageDisabled) ||
      profile->GetPrefs()->GetBoolean(disks::prefs::kExternalStorageReadOnly)) {
    return RespondNow(Error(image_writer::error::kDeviceWriteError));
  }
#endif
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 3);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[2].is_string());

  const std::string& storage_unit_id = args()[0].GetString();
  const std::string& filesystem_name = args()[1].GetString();
  const std::string& filesystem_path = args()[2].GetString();

  base::FilePath path;

  std::string error;
  if (!extensions::app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(), &path,
          &error)) {
    return RespondNow(Error(std::move(error)));
  }

  image_writer::OperationManager::Get(browser_context())
      ->StartWriteFromFile(
          extension_id(), path, storage_unit_id,
          base::BindOnce(&ImageWriterPrivateWriteFromFileFunction::OnComplete,
                         this));

  return RespondLater();
}

ImageWriterPrivateCancelWriteFunction::ImageWriterPrivateCancelWriteFunction() =
    default;

ImageWriterPrivateCancelWriteFunction::
    ~ImageWriterPrivateCancelWriteFunction() = default;

ExtensionFunction::ResponseAction ImageWriterPrivateCancelWriteFunction::Run() {
  image_writer::OperationManager::Get(browser_context())
      ->CancelWrite(
          extension_id(),
          base::BindOnce(&ImageWriterPrivateCancelWriteFunction::OnComplete,
                         this));

  return RespondLater();
}

ImageWriterPrivateDestroyPartitionsFunction::
    ImageWriterPrivateDestroyPartitionsFunction() = default;

ImageWriterPrivateDestroyPartitionsFunction::
    ~ImageWriterPrivateDestroyPartitionsFunction() = default;

ExtensionFunction::ResponseAction
ImageWriterPrivateDestroyPartitionsFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->GetPrefs()->GetBoolean(disks::prefs::kExternalStorageDisabled) ||
      profile->GetPrefs()->GetBoolean(disks::prefs::kExternalStorageReadOnly)) {
    return RespondNow(Error(image_writer::error::kDeviceWriteError));
  }
#endif

  std::optional<image_writer_api::DestroyPartitions::Params> params =
      image_writer_api::DestroyPartitions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  image_writer::OperationManager::Get(browser_context())
      ->DestroyPartitions(
          extension_id(), params->storage_unit_id,
          base::BindOnce(
              &ImageWriterPrivateDestroyPartitionsFunction::OnComplete, this));

  return RespondLater();
}

ImageWriterPrivateListRemovableStorageDevicesFunction::
    ImageWriterPrivateListRemovableStorageDevicesFunction() = default;

ImageWriterPrivateListRemovableStorageDevicesFunction::
    ~ImageWriterPrivateListRemovableStorageDevicesFunction() = default;

ExtensionFunction::ResponseAction
ImageWriterPrivateListRemovableStorageDevicesFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->GetPrefs()->GetBoolean(disks::prefs::kExternalStorageDisabled)) {
    // Return an empty device list.
    OnDeviceListReady(base::MakeRefCounted<StorageDeviceList>());
    return AlreadyResponded();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  RemovableStorageProvider::GetAllDevices(base::BindOnce(
      &ImageWriterPrivateListRemovableStorageDevicesFunction::OnDeviceListReady,
      this));

  return RespondLater();
}

void ImageWriterPrivateListRemovableStorageDevicesFunction::OnDeviceListReady(
    scoped_refptr<StorageDeviceList> device_list) {
  const bool success = device_list.get() != nullptr;
  if (success) {
    Respond(ArgumentList(
        image_writer_api::ListRemovableStorageDevices::Results::Create(
            device_list->data)));
  } else {
    Respond(Error(image_writer::error::kDeviceListError));
  }
}

}  // namespace extensions
