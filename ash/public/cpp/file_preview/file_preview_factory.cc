// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/file_preview/file_preview_factory.h"

#include "ash/public/cpp/file_preview/file_preview_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/types/pass_key.h"

namespace ash {

// static
FilePreviewFactory* FilePreviewFactory::Get() {
  static base::NoDestructor<FilePreviewFactory> instance;
  return instance.get();
}

FilePreviewController* FilePreviewFactory::GetController(
    const ui::ImageModel& model) {
  // All `ui::ImageModel`s created by this factory are
  // `ui::ImageModel::ImageGenerator`s.
  if (!model.IsImageGenerator()) {
    return nullptr;
  }
  const auto* key = FilePreviewController::GetKey(model);
  const auto controller_iter = registry_.find(key);
  return controller_iter != registry_.end() ? controller_iter->second : nullptr;
}

ui::ImageModel FilePreviewFactory::CreateImageModel(base::FilePath path,
                                                    gfx::Size size) {
  auto controller = std::make_unique<FilePreviewController>(
      base::PassKey<FilePreviewFactory>(), std::move(path), size);

  const auto* key = FilePreviewController::GetKey(
      controller->GetImageSkia(base::PassKey<FilePreviewFactory>()));
  registry_.emplace(key, controller.get());

  // Take advantage of `base::ScopedClosureRunner` to remove the new
  // `FilePreviewController` from `registry_` on its destruction.
  auto registry_cleanup = base::ScopedClosureRunner(base::BindOnce(
      [](FilePreviewController::Key key,
         const base::WeakPtr<FilePreviewFactory>& self) {
        if (self) {
          self->registry_.erase(key);
        }
      },
      key, weak_factory_.GetMutableWeakPtr()));

  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(
          [](FilePreviewController* controller,
             const base::ScopedClosureRunner&, const ui::ColorProvider*) {
            return controller->GetImageSkia(
                base::PassKey<FilePreviewFactory>());
          },
          base::Owned(std::move(controller)), std::move(registry_cleanup)),
      std::move(size));
}

FilePreviewFactory::FilePreviewFactory() = default;
FilePreviewFactory::~FilePreviewFactory() = default;

}  // namespace ash
