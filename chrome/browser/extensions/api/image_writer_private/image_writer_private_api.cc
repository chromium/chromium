// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_private_api.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "build/chromeos_buildflags.h"
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
  if (success)
    Respond(NoArguments());
  else
    Respond(Error(error));
}

ImageWriterPrivateWriteFromUrlFunction::
    ImageWriterPrivateWriteFromUrlFunction() {
}

ImageWriterPrivateWriteFromUrlFunction::
    ~ImageWriterPrivateWriteFromUrlFunction() {
}

ExtensionFunction::ResponseAction
ImageWriterPrivateWriteFromUrlFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled) ||
      profile->GetPrefs()->GetBoolean(prefs::kExternalStorageReadOnly)) {
    return RespondNow(Error(image_writer::error::kDeviceWriteError));
  }
#endif
  std::unique_ptr<image_writer_api::WriteFromUrl::Params> params(
      image_writer_api::WriteFromUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url(params->image_url);
  if (!url.is_valid())
    return RespondNow(Error(image_writer::error::kUrlInvalid));

  std::string hash;
  if (params->options.get() && params->options->image_hash.get()) {
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
    ImageWriterPrivateWriteFromFileFunction() {
}

ImageWriterPrivateWriteFromFileFunction::
    ~ImageWriterPrivateWriteFromFileFunction() {
}

ExtensionFunction::ResponseAction
ImageWriterPrivateWriteFromFileFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled) ||
      profile->GetPrefs()->GetBoolean(prefs::kExternalStorageReadOnly)) {
    return RespondNow(Error(image_writer::error::kDeviceWriteError));
  }
#endif
  std::string filesystem_name;
  std::string filesystem_path;
  std::string storage_unit_id;

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &storage_unit_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &filesystem_path));

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

ImageWriterPrivateCancelWriteFunction::ImageWriterPrivateCancelWriteFunction() {
}

ImageWriterPrivateCancelWriteFunction::
    ~ImageWriterPrivateCancelWriteFunction() {
}

ExtensionFunction::ResponseAction ImageWriterPrivateCancelWriteFunction::Run() {
  image_writer::OperationManager::Get(browser_context())
      ->CancelWrite(
          extension_id(),
          base::BindOnce(&ImageWriterPrivateCancelWriteFunction::OnComplete,
                         this));
  return RespondLater();
}

ImageWriterPrivateDestroyPartitionsFunction::
    ImageWriterPrivateDestroyPartitionsFunction() {
}

ImageWriterPrivateDestroyPartitionsFunction::
    ~ImageWriterPrivateDestroyPartitionsFunction() {
}

ExtensionFunction::ResponseAction
ImageWriterPrivateDestroyPartitionsFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled) ||
      profile->GetPrefs()->GetBoolean(prefs::kExternalStorageReadOnly)) {
    return RespondNow(Error(image_writer::error::kDeviceWriteError));
  }

#endif
  std::unique_ptr<image_writer_api::DestroyPartitions::Params> params(
      image_writer_api::DestroyPartitions::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  image_writer::OperationManager::Get(browser_context())
      ->DestroyPartitions(
          extension_id(), params->storage_unit_id,
          base::BindOnce(
              &ImageWriterPrivateDestroyPartitionsFunction::OnComplete, this));
  return RespondLater();
}

ImageWriterPrivateListRemovableStorageDevicesFunction::
  ImageWriterPrivateListRemovableStorageDevicesFunction() {
}

ImageWriterPrivateListRemovableStorageDevicesFunction::
  ~ImageWriterPrivateListRemovableStorageDevicesFunction() {
}

ExtensionFunction::ResponseAction
ImageWriterPrivateListRemovableStorageDevicesFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    // Return an empty device list.
    OnDeviceListReady(base::MakeRefCounted<StorageDeviceList>());
    return AlreadyResponded();
  }
#endif
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
