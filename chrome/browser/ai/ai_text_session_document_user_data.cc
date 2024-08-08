// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session_document_user_data.h"

#include "base/functional/bind.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "content/public/browser/render_frame_host.h"

AITextSessionDocumentUserData::AITextSessionDocumentUserData(
    content::RenderFrameHost* rfh,
    std::unique_ptr<AITextSession> session)
    : content::DocumentUserData<AITextSessionDocumentUserData>(rfh),
      session_(std::move(session)) {
  session_->SetDisconnectHandler(
      base::BindOnce([](content::RenderFrameHost* rfh,
                        AITextSession* _) { DeleteForCurrentDocument(rfh); },
                     base::Unretained(rfh)));
}

AITextSessionDocumentUserData::~AITextSessionDocumentUserData() = default;

DOCUMENT_USER_DATA_KEY_IMPL(AITextSessionDocumentUserData);
