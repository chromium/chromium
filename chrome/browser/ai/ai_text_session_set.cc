// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session_set.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

namespace {

const char kAITestSessionSetSupportsUserDataKey[] = "ai_text_session_set";

}  // namespace

// The following two classes ensure the `AITextSessionSet` and the
// `AITextSessions`s will be destroyed when the document or worker host is gone.
// `AITextSessionSetSupportsUserData` is for workers and
// `AITextSessionSetDocumentUserData` is for document.
class AITextSessionSetSupportsUserData : public AITextSessionSet,
                                         public base::SupportsUserData::Data {
 public:
  explicit AITextSessionSetSupportsUserData(base::SupportsUserData* host)
      : host_(host) {}
  ~AITextSessionSetSupportsUserData() override = default;

  static AITextSessionSetSupportsUserData* GetOrCreateFor(
      base::PassKey<AITextSessionSet> pass_key,
      base::SupportsUserData* host) {
    if (!host->GetUserData(kAITestSessionSetSupportsUserDataKey)) {
      host->SetUserData(
          kAITestSessionSetSupportsUserDataKey,
          std::make_unique<AITextSessionSetSupportsUserData>(host));
    }
    return static_cast<AITextSessionSetSupportsUserData*>(
        host->GetUserData(kAITestSessionSetSupportsUserDataKey));
  }

 protected:
  void OnAllSessionsRemoved() override {
    host_->RemoveUserData(kAITestSessionSetSupportsUserDataKey);
  }

 private:
  raw_ptr<base::SupportsUserData> host_;
};

class AITextSessionSetDocumentUserData
    : public AITextSessionSet,
      public content::DocumentUserData<AITextSessionSetDocumentUserData> {
 public:
  ~AITextSessionSetDocumentUserData() override = default;

 protected:
  explicit AITextSessionSetDocumentUserData(content::RenderFrameHost* rfh)
      : content::DocumentUserData<AITextSessionSetDocumentUserData>(rfh) {}

  void OnAllSessionsRemoved() override {
    content::RemoveDocumentUserData(&render_frame_host(),
                                    kAITestSessionSetSupportsUserDataKey);
  }

 private:
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(AITextSessionSetDocumentUserData);

AITextSessionSet::AITextSessionSet() = default;
AITextSessionSet::~AITextSessionSet() = default;

void AITextSessionSet::AddSession(std::unique_ptr<AITextSession> session) {
  // Removes the `session` from the `AITextSessionSet` if the
  // connection is gone before the document or worker host is destroyed.
  // The `AITextSessionSet` is stored in the context of the receiver
  // for `blink::mojom::AIManager`, and the `AITextSession`s are owned
  // by it.
  // At this point the `this` should not be destroyed, otherwise it
  // will also destroy all the sessions in the set, and prevent this
  // `disconnect_handler` from execution, because the receiver of the
  // connection is owned by the `AITextSession`.
  session->SetDisconnectHandler(
      base::BindOnce(&AITextSessionSet::RemoveSession, base::Unretained(this),
                     base::Unretained(session.get())));
  sessions_.insert(std::move(session));
}

void AITextSessionSet::RemoveSession(AITextSession* session) {
  sessions_.erase(session);

  if (sessions_.empty()) {
    OnAllSessionsRemoved();
  }
}

AITextSessionSet* AITextSessionSet::GetFromContext(ReceiverContext context) {
  // For document, the session set is wrapped as a `DocumentUserData`.
  if (std::holds_alternative<content::RenderFrameHost*>(context)) {
    return AITextSessionSetDocumentUserData::GetOrCreateForCurrentDocument(
        std::get<content::RenderFrameHost*>(context));
  }

  // For workers, the session will be stored in the `AITextSessionSet` that's
  // attached as a `SupportsUserData::Data`.
  CHECK(std::holds_alternative<base::SupportsUserData*>(context));
  return AITextSessionSetSupportsUserData::GetOrCreateFor(
      base::PassKey<AITextSessionSet>(),
      std::get<base::SupportsUserData*>(context));
}

size_t AITextSessionSet::GetSessionSetSizeForTesting() {
  return sessions_.size();
}
