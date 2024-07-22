// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/cros_apps_key_event_handler.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

CrosAppsKeyEventHandler::CrosAppsKeyEventHandler(Profile* profile)
    : profile_(*profile) {
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kAccessibility);
}

CrosAppsKeyEventHandler::~CrosAppsKeyEventHandler() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
}

void CrosAppsKeyEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  // Hard-coded url for demo app scope.
  const GURL url = GURL(
      "isolated-app://"
      "uwsszrmaowqmxw4f262x5jozzhe5bc4tefqfa5lado674o462aoaaaic/");

  blink::TransferableMessage msg =
      blink::EncodeWebMessagePayload(base::UTF8ToUTF16(event->GetCodeString()));
  msg.sender_agent_cluster_id =
      blink::WebMessagePort::GetEmbedderAgentClusterID();
  profile_->GetStoragePartitionForUrl(url)
      ->GetServiceWorkerContext()
      ->StartServiceWorkerAndDispatchMessage(
          url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
          std::move(msg), base::DoNothing());
}
