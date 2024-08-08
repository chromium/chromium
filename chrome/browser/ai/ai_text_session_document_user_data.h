// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEXT_SESSION_DOCUMENT_USER_DATA_H_
#define CHROME_BROWSER_AI_AI_TEXT_SESSION_DOCUMENT_USER_DATA_H_

#include "chrome/browser/ai/ai_text_session.h"
#include "content/public/browser/document_user_data.h"

// The implementation of `blink::mojom::ModelGenericSession`, which exposes the
// single stream-based `Execute()` API for model execution.
class AITextSessionDocumentUserData
    : public content::DocumentUserData<AITextSessionDocumentUserData> {
 public:
  AITextSessionDocumentUserData(content::RenderFrameHost* rfh,
                                std::unique_ptr<AITextSession> session);
  AITextSessionDocumentUserData(const AITextSessionDocumentUserData&) = delete;
  AITextSessionDocumentUserData& operator=(
      const AITextSessionDocumentUserData&) = delete;
  ~AITextSessionDocumentUserData() override;

 private:
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  std::unique_ptr<AITextSession> session_;
};

#endif  // CHROME_BROWSER_AI_AI_TEXT_SESSION_DOCUMENT_USER_DATA_H_
