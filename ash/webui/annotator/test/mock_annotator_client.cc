// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/webui/annotator/test/mock_annotator_client.h"

#include <memory>

#include "ash/public/cpp/annotator/annotations_overlay_view.h"

namespace ash {
namespace {

class TestAnnotationsOverlayView : public AnnotationsOverlayView {
 public:
  TestAnnotationsOverlayView() = default;
  TestAnnotationsOverlayView(const TestAnnotationsOverlayView&) = delete;
  TestAnnotationsOverlayView& operator=(const TestAnnotationsOverlayView&) =
      delete;
  ~TestAnnotationsOverlayView() override = default;
};

}  // namespace

MockAnnotatorClient::MockAnnotatorClient() = default;
MockAnnotatorClient::~MockAnnotatorClient() = default;

std::unique_ptr<AnnotationsOverlayView>
MockAnnotatorClient::CreateAnnotationsOverlayView() const {
  return std::make_unique<TestAnnotationsOverlayView>();
}
}  // namespace ash
