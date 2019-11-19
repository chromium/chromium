// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_private_api.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"

namespace image_writer_api = extensions::api::image_writer_private;

namespace extensions {

ImageWriterPrivateBaseFunction::ImageWriterPrivateBaseFunction() {}

ImageWriterPrivateBaseFunction::~ImageWriterPrivateBaseFunction() {}

void ImageWriterPrivateBaseFunction::OnComplete(bool success,
                                                const std::string& error) {
  if (!success)
    error_ = error;
  SendResponse(success);
}

ImageWriterPrivateWriteFromUrlFunction::
    ImageWriterPrivateWriteFromUrlFunction() {
}

ImageWriterPrivateWriteFromUrlFunction::
    ~ImageWriterPrivateWriteFromUrlFunction() {
}

bool ImageWriterPrivateWriteFromUrlFunction::RunAsync() {
#if defined(OS_CHROMEOS)
  if (GetProfile()->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled) ||
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExternalStorageReadOnly)) {
    error_ = image_writer::error::kDeviceWriteError;
    return false;
  }
#endif
  std::unique_ptr<image_writer_api::WriteFromUrl::Params> params(
      image_writer_api::WriteFromUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url(params->image_url);
  if (!url.is_valid()) {
    error_ = image_writer::error::kUrlInvalid;
    return false;
  }

  std::string hash;
  if (params->options.get() && params->options->image_hash.get()) {
    hash = *params->options->image_hash;
  }

  image_writer::OperationManager::Get(GetProfile())
      ->StartWriteFromUrl(
          extension_id(), url, hash, params->storage_unit_id,
          base::BindOnce(&ImageWriterPrivateWriteFromUrlFunction::OnComplete,
                         this));
  return true;
}

ImageWriterPrivateWriteFromFileFunction::
    ImageWriterPrivateWriteFromFileFunction() {
}

ImageWriterPrivateWriteFromFileFunction::
    ~ImageWriterPrivateWriteFromFileFunction() {
}

bool ImageWriterPrivateWriteFromFileFunction::RunAsync() {
#if defined(OS_CHROMEOS)
  if (GetProfile()->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled) ||
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExternalStorageReadOnly)) {
    error_ = image_writer::error::kDeviceWriteError;
    return false;
  }
#endif
  std::string filesystem_name;
  std::string filesystem_path;
  std::string storage_unit_id;

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &storage_unit_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &filesystem_path));

  base::FilePath path;

  if (!extensions::app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(), &path,
          &error_))
    return false;

  image_writer::OperationManager::Get(GetProfile())
      ->StartWriteFromFile(
          extension_id(), path, storage_unit_id,
          base::BindOnce(&ImageWriterPrivateWriteFromFileFunction::OnComplete,
                         this));
  return true;
}

ImageWriterPrivateCancelWriteFunction::ImageWriterPrivateCancelWriteFunction() {
}

ImageWriterPrivateCancelWriteFunction::
    ~ImageWriterPrivateCancelWriteFunction() {
}

bool ImageWriterPrivateCancelWriteFunction::RunAsync() {
  image_writer::OperationManager::Get(GetProfile())
      ->CancelWrite(
          extension_id(),
          base::BindOnce(&ImageWriterPrivateCancelWriteFunction::OnComplete,
                         this));
  return true;
}

ImageWriterPrivateDestroyPartitionsFunction::
    ImageWriterPrivateDestroyPartitionsFunction() {
}

ImageWriterPrivateDestroyPartitionsFunction::
    ~ImageWriterPrivateDestroyPartitionsFunction() {
}

bool ImageWriterPrivateDestroyPartitionsFunction::RunAsync() {
#if defined(OS_CHROMEOS)
  if (GetProfile()->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled) ||
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExternalStorageReadOnly)) {
    error_ = image_writer::error::kDeviceWriteError;
    return false;
  }

#endif
  std::unique_ptr<image_writer_api::DestroyPartitions::Params> params(
      image_writer_api::DestroyPartitions::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  image_writer::OperationManager::Get(GetProfile())
      ->DestroyPartitions(
          extension_id(), params->storage_unit_id,
          base::BindOnce(
              &ImageWriterPrivateDestroyPartitionsFunction::OnComplete, this));
  return true;
}

ImageWriterPrivateListRemovableStorageDevicesFunction::
  ImageWriterPrivateListRemovableStorageDevicesFunction() {
}

ImageWriterPrivateListRemovableStorageDevicesFunction::
  ~ImageWriterPrivateListRemovableStorageDevicesFunction() {
}

bool ImageWriterPrivateListRemovableStorageDevicesFunction::RunAsync() {
#if defined(OS_CHROMEOS)
  if (GetProfile()->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    // Return an empty device list.
    OnDeviceListReady(base::MakeRefCounted<StorageDeviceList>());
    return true;
  }
#endif
  RemovableStorageProvider::GetAllDevices(base::BindOnce(
      &ImageWriterPrivateListRemovableStorageDevicesFunction::OnDeviceListReady,
      this));
  return true;
}

void ImageWriterPrivateListRemovableStorageDevicesFunction::OnDeviceListReady(
    scoped_refptr<StorageDeviceList> device_list) {
  const bool success = device_list.get() != nullptr;
  if (success) {
    results_ = image_writer_api::ListRemovableStorageDevices::Results::Create(
        device_list->data);
    SendResponse(true);
  } else {
    error_ = image_writer::error::kDeviceListError;
    SendResponse(false);
  }
}

}  // namespace extensions
