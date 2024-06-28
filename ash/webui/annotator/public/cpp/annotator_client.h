// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ANNOTATOR_PUBLIC_CPP_ANNOTATOR_CLIENT_H_
#define ASH_WEBUI_ANNOTATOR_PUBLIC_CPP_ANNOTATOR_CLIENT_H_

#include <memory>

namespace ash {

class UntrustedAnnotatorPageHandlerImpl;
struct AnnotatorTool;
class AnnotationsOverlayView;

// Defines interface to access Browser side functionalities for the
// Annotator tool.
class AnnotatorClient {
 public:
  AnnotatorClient(const AnnotatorClient&) = delete;
  AnnotatorClient& operator=(const AnnotatorClient&) = delete;

  static AnnotatorClient* Get();

  // Registers the AnnotatorPageHandlerImpl that is owned by the WebUI that
  // contains the annotator.
  virtual void SetAnnotatorPageHandler(
      UntrustedAnnotatorPageHandlerImpl* handler) = 0;

  // Resets the stored AnnotatorPageHandlerImpl if it matches the one that is
  // passed in.
  virtual void ResetAnnotatorPageHandler(
      UntrustedAnnotatorPageHandlerImpl* handler) = 0;

  // Sets the tool inside the annotator WebUI.
  virtual void SetTool(const AnnotatorTool& tool) = 0;

  // Clears the contents of the annotator canvas.
  virtual void Clear() = 0;

  // Creates and returns the view that will be used as the contents view of the
  // overlay widget, which is added as a child of the surface on which the
  // annotations are triggered to host annotations.
  virtual std::unique_ptr<AnnotationsOverlayView> CreateAnnotationsOverlayView()
      const = 0;

 protected:
  AnnotatorClient();
  virtual ~AnnotatorClient();
};

}  // namespace ash

#endif  // ASH_WEBUI_ANNOTATOR_PUBLIC_CPP_ANNOTATOR_CLIENT_H_
