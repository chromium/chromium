// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CLIPBOARD_IMAGE_MODEL_FACTORY_H_
#define ASH_PUBLIC_CPP_CLIPBOARD_IMAGE_MODEL_FACTORY_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "ui/base/models/image_model.h"

namespace ui {
class ImageModel;
}  // namespace ui

namespace ash {

// A factory implemented in the Browser with the primary profile which is
// responsible for creating ui::ImageModels out of html strings from
// ClipboardHistory to work around dependency restrictions in ash. This will not
// create ImageModels until it is activated.
class ASH_PUBLIC_EXPORT ClipboardImageModelFactory {
 public:
  virtual ~ClipboardImageModelFactory();

  // Returns the singleton factory instance.
  static ClipboardImageModelFactory* Get();

  using ImageModelCallback = base::OnceCallback<void(ui::ImageModel)>;

  // Asynchronously renders |html_markup|, identified by |id|. Later the
  // rendered html will be returned via |callback|, which is called as long as
  // the request is not canceled via CancelRequest. If the request times out, an
  // empty ImageModel will be passed through |callback|.
  virtual void Render(const base::UnguessableToken& id,
                      const std::string& html_markup,
                      const gfx::Size& bounding_box_size,
                      ImageModelCallback callback) = 0;

  // Called to stop rendering which was requested with |id|.
  virtual void CancelRequest(const base::UnguessableToken& id) = 0;

  // Until Activate() is called, ClipboardImageModelFactory is in an inactive
  // state and all rendering requests will be queued until activated.
  virtual void Activate() = 0;

  // Called after Activate() to pause rendering requests. Rendering requests
  // will will continue until all requests are processed if
  // RenderCurrentPendingRequests() has been called.
  virtual void Deactivate() = 0;

  // Called to render all currently pending requests. This is called when the
  // virtual keyboard private api calls getClipboardHistory, so that clipboard
  // history items that are displayed will be rendered. Since there is no way to
  // track when the clipboard history is shown/hidden in the virtual keyboard,
  // this is called to ensure the current clipboard history is rendered.
  // Convenience function to `Activate()` until all requests are finished.
  virtual void RenderCurrentPendingRequests() = 0;

  // Called during shutdown to cleanup references to Profile.
  virtual void OnShutdown() = 0;

 protected:
  ClipboardImageModelFactory();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CLIPBOARD_IMAGE_MODEL_FACTORY_H_
