// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FILE_PREVIEW_FILE_PREVIEW_FACTORY_H_
#define ASH_PUBLIC_CPP_FILE_PREVIEW_FILE_PREVIEW_FACTORY_H_

#include <map>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/file_preview/file_preview_controller.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "ui/base/models/image_model.h"

namespace ash {

// DO NOT USE: Work in progress.
// Used to generate `ui::ImageModel`s capable of representing a preview of a
// file. Currently only supports gif files, though other types of images may
// work (http://b/266000882).
class ASH_PUBLIC_EXPORT FilePreviewFactory {
 public:
  FilePreviewFactory(const FilePreviewFactory&) = delete;
  FilePreviewFactory& operator=(const FilePreviewFactory&) = delete;

  // Gets the singleton instance of this class.
  static FilePreviewFactory* Get();

  // Fetches the controller for a given file preview image model.
  // If the given `ui::ImageModel` was not created through `CreateImageModel()`,
  // this will return a nullptr.
  [[nodiscard]] FilePreviewController* GetController(
      const ui::ImageModel& model);

  const std::map<FilePreviewController::Key, FilePreviewController*>&
  GetRegistryForTest() const {
    return registry_;
  }

 private:
  friend base::NoDestructor<FilePreviewFactory>;
  friend class FilePreviewFactoryTest;
  friend class FilePreviewTest;

  FilePreviewFactory();
  ~FilePreviewFactory();

  // Creates a `ui::ImageModel` with a representation of a file. The controller
  // for this model can be fetched by calling `GetController()`.
  // TODO(http://b/266000155): Make public upon code completion.
  [[nodiscard]] ui::ImageModel CreateImageModel(base::FilePath path,
                                                gfx::Size size);

  // Maps `FilePreviewController::Key` of a `ui::ImageModel` to its matching
  // `FilePreviewController`.
  std::map<FilePreviewController::Key, FilePreviewController*> registry_;

  base::WeakPtrFactory<FilePreviewFactory> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FILE_PREVIEW_FILE_PREVIEW_FACTORY_H_
