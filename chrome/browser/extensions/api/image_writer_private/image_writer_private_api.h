// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_PRIVATE_API_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "extensions/browser/extension_function.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#endif

namespace extensions {

class ImageWriterPrivateBaseFunction : public ExtensionFunction {
 public:
  ImageWriterPrivateBaseFunction();

  ImageWriterPrivateBaseFunction(const ImageWriterPrivateBaseFunction&) =
      delete;
  ImageWriterPrivateBaseFunction& operator=(
      const ImageWriterPrivateBaseFunction&) = delete;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  virtual void OnComplete(const std::optional<std::string>& error);
#else
  virtual void OnComplete(bool success, const std::string& error);
#endif

 protected:
  ~ImageWriterPrivateBaseFunction() override;
};

class ImageWriterPrivateWriteFromUrlFunction
    : public ImageWriterPrivateBaseFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.writeFromUrl",
                             IMAGEWRITER_WRITEFROMURL)
  ImageWriterPrivateWriteFromUrlFunction();

 private:
  ~ImageWriterPrivateWriteFromUrlFunction() override;
  ResponseAction Run() override;
};

class ImageWriterPrivateWriteFromFileFunction
    : public ImageWriterPrivateBaseFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.writeFromFile",
                             IMAGEWRITER_WRITEFROMFILE)
  ImageWriterPrivateWriteFromFileFunction();

 private:
  ~ImageWriterPrivateWriteFromFileFunction() override;
  ResponseAction Run() override;
};

class ImageWriterPrivateCancelWriteFunction
    : public ImageWriterPrivateBaseFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.cancelWrite",
                             IMAGEWRITER_CANCELWRITE)
  ImageWriterPrivateCancelWriteFunction();

 private:
  ~ImageWriterPrivateCancelWriteFunction() override;
  ResponseAction Run() override;
};

class ImageWriterPrivateDestroyPartitionsFunction
    : public ImageWriterPrivateBaseFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.destroyPartitions",
                             IMAGEWRITER_DESTROYPARTITIONS)
  ImageWriterPrivateDestroyPartitionsFunction();

 private:
  ~ImageWriterPrivateDestroyPartitionsFunction() override;
  ResponseAction Run() override;
};

class ImageWriterPrivateListRemovableStorageDevicesFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.listRemovableStorageDevices",
                             IMAGEWRITER_LISTREMOVABLESTORAGEDEVICES)
  ImageWriterPrivateListRemovableStorageDevicesFunction();

 private:
  ~ImageWriterPrivateListRemovableStorageDevicesFunction() override;
  ResponseAction Run() override;
  void OnDeviceListReady(scoped_refptr<StorageDeviceList> device_list);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnCrosapiDeviceListReady(
      std::optional<std::vector<crosapi::mojom::RemovableStorageDevicePtr>>
          devices);
#endif
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_PRIVATE_API_H_
