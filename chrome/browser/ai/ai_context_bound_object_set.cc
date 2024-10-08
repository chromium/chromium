// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_context_bound_object_set.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

namespace {

const char kAIContextBoundObjectSetUserDataKey[] = "ai_context_bound_objects";

}  // namespace

// The following two classes ensure the `AIContextBoundObjectSet` and the
// `AIContextBoundObject`s will be destroyed when the document or worker host is
// gone. `AIContextBoundObjectSetSupportsUserData` is for workers and
// `AIContextBoundObjectSetDocumentUserData` is for document.
class AIContextBoundObjectSetSupportsUserData
    : public AIContextBoundObjectSet,
      public base::SupportsUserData::Data {
 public:
  explicit AIContextBoundObjectSetSupportsUserData(base::SupportsUserData* host)
      : host_(host) {}
  ~AIContextBoundObjectSetSupportsUserData() override = default;

  static AIContextBoundObjectSetSupportsUserData* GetOrCreateFor(
      base::PassKey<AIContextBoundObjectSet> pass_key,
      base::SupportsUserData* host) {
    if (!host->GetUserData(kAIContextBoundObjectSetUserDataKey)) {
      host->SetUserData(
          kAIContextBoundObjectSetUserDataKey,
          std::make_unique<AIContextBoundObjectSetSupportsUserData>(host));
    }
    return static_cast<AIContextBoundObjectSetSupportsUserData*>(
        host->GetUserData(kAIContextBoundObjectSetUserDataKey));
  }

 private:
  raw_ptr<base::SupportsUserData> host_;
};

class AIContextBoundObjectSetDocumentUserData
    : public AIContextBoundObjectSet,
      public content::DocumentUserData<
          AIContextBoundObjectSetDocumentUserData> {
 public:
  ~AIContextBoundObjectSetDocumentUserData() override = default;

 protected:
  explicit AIContextBoundObjectSetDocumentUserData(
      content::RenderFrameHost* rfh)
      : content::DocumentUserData<AIContextBoundObjectSetDocumentUserData>(
            rfh) {}

 private:
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(AIContextBoundObjectSetDocumentUserData);

// static
AIContextBoundObjectSet::ReceiverContextRawRef
AIContextBoundObjectSet::ToReceiverContextRawRef(ReceiverContext context) {
  if (std::holds_alternative<content::RenderFrameHost*>(context)) {
    return raw_ref<content::RenderFrameHost>(
        *std::get<content::RenderFrameHost*>(context));
  }
  CHECK(std::holds_alternative<base::SupportsUserData*>(context));
  return raw_ref<base::SupportsUserData>(
      *std::get<base::SupportsUserData*>(context));
}

// static
AIContextBoundObjectSet::ReceiverContext
AIContextBoundObjectSet::ToReceiverContext(
    ReceiverContextRawRef context_raw_ref) {
  if (std::holds_alternative<raw_ref<content::RenderFrameHost>>(
          context_raw_ref)) {
    return &std::get<raw_ref<content::RenderFrameHost>>(context_raw_ref).get();
  }
  CHECK(
      std::holds_alternative<raw_ref<base::SupportsUserData>>(context_raw_ref));
  return &std::get<raw_ref<base::SupportsUserData>>(context_raw_ref).get();
}

AIContextBoundObjectSet::AIContextBoundObjectSet() = default;
AIContextBoundObjectSet::~AIContextBoundObjectSet() = default;

void AIContextBoundObjectSet::AddContextBoundObject(
    std::unique_ptr<AIContextBoundObject> object) {
  // Removes the `object` from the `AIContextBoundObjectSet` if the
  // deletion callback is called before the document or worker host is
  // destroyed.
  // The `AIContextBoundObjectSet` is stored in the context of the receiver for
  // `blink::mojom::AIManager`, and the AIContextBoundObject objects are owned
  // by it. At this point the `this` should not be destroyed, otherwise it will
  // also destroy all the objects in the set, and prevent this
  // `deletion_callback` from execution. The deletion callback is set to the
  // AIContextBoundObject object in SetDeletionCallback and should not be called
  // if the AIContextBoundObject object has been destructed.
  object->SetDeletionCallback(
      base::BindOnce(&AIContextBoundObjectSet::RemoveContextBoundObject,
                     base::Unretained(this), base::Unretained(object.get())));
  context_bound_object_set_.insert(std::move(object));
}

void AIContextBoundObjectSet::RemoveContextBoundObject(
    AIContextBoundObject* object) {
  context_bound_object_set_.erase(object);
}

AIContextBoundObjectSet* AIContextBoundObjectSet::GetFromContext(
    ReceiverContext context) {
  // For document, the set is wrapped as a `DocumentUserData`.
  if (std::holds_alternative<content::RenderFrameHost*>(context)) {
    return AIContextBoundObjectSetDocumentUserData::
        GetOrCreateForCurrentDocument(
            std::get<content::RenderFrameHost*>(context));
  }

  // For workers, the context bound objects will be stored in the
  // `AIContextBoundObjectSet` that's attached as a `SupportsUserData::Data`.
  CHECK(std::holds_alternative<base::SupportsUserData*>(context));
  return AIContextBoundObjectSetSupportsUserData::GetOrCreateFor(
      base::PassKey<AIContextBoundObjectSet>(),
      std::get<base::SupportsUserData*>(context));
}

size_t AIContextBoundObjectSet::GetSizeForTesting() {
  return context_bound_object_set_.size();
}
