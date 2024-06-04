// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ANNOTATOR_TEST_MOCK_ANNOTATOR_CLIENT_H_
#define ASH_WEBUI_ANNOTATOR_TEST_MOCK_ANNOTATOR_CLIENT_H_

#include "ash/webui/annotator/annotator_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAnnotatorClient : public AnnotatorClient {
 public:
  MockAnnotatorClient();
  MockAnnotatorClient(const MockAnnotatorClient&) = delete;
  MockAnnotatorClient& operator=(const MockAnnotatorClient&) = delete;
  ~MockAnnotatorClient() override;

  // AnnotatorClient:
  MOCK_METHOD1(SetAnnotatorPageHandler,
               void(UntrustedAnnotatorPageHandlerImpl*));
  MOCK_METHOD1(ResetAnnotatorPageHandler,
               void(UntrustedAnnotatorPageHandlerImpl*));
  MOCK_METHOD1(SetTool, void(const AnnotatorTool&));
  MOCK_METHOD0(Clear, void());
};

}  // namespace ash

#endif  // ASH_WEBUI_ANNOTATOR_TEST_MOCK_ANNOTATOR_CLIENT_H_
