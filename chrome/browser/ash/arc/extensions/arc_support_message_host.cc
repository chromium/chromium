// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/extensions/arc_support_message_host.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"

namespace arc {

// static
const char ArcSupportMessageHost::kHostName[] = "com.google.arc_support";

// static
const char* const ArcSupportMessageHost::kHostOrigin[] = {
    "chrome-extension://cnbgggchhmkkdmeppjobngjoejnihlei/"};

// static
std::unique_ptr<extensions::NativeMessageHost> ArcSupportMessageHost::Create(
    content::BrowserContext* browser_context) {
  return std::unique_ptr<NativeMessageHost>(new ArcSupportMessageHost());
}

ArcSupportMessageHost::ArcSupportMessageHost() = default;

ArcSupportMessageHost::~ArcSupportMessageHost() {
  // On shutdown, ArcSessionManager may be already deleted. In that case,
  // ArcSessionManager::Get() returns nullptr. Note that, ArcSupportHost
  // disconnects to this instance on shutdown already.
  ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
  if (arc_session_manager) {
    DCHECK(arc_session_manager->support_host());
    arc_session_manager->support_host()->UnsetMessageHost(this);
  }
}

void ArcSupportMessageHost::SendMessage(const base::ValueView& message) {
  if (!client_)
    return;

  std::string message_string;
  base::JSONWriter::Write(message, &message_string);
  client_->PostMessageFromNativeHost(message_string);
}

void ArcSupportMessageHost::SetObserver(Observer* observer) {
  // We assume that the observer instance is only ArcSupportHost, which is
  // currently system unique. This is also used to reset the observere,
  // so |observer| xor |observer_| needs to be nullptr.
  DCHECK(!observer != !observer_);
  observer_ = observer;
}

void ArcSupportMessageHost::Start(Client* client) {
  DCHECK(!client_);
  client_ = client;

  ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
  if (arc_session_manager) {
    DCHECK(arc_session_manager->support_host());
    arc_session_manager->support_host()->SetMessageHost(this);
  }
}

void ArcSupportMessageHost::OnMessage(const std::string& message_string) {
  if (!observer_)
    return;

  // |message_string| comes from the ARC support extension via native messaging,
  // which on Chrome OS runs in the browser process.
  // Therefore this use of JSONReader does not violate
  // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md.
  std::optional<base::Value> message = base::JSONReader::Read(message_string);
  if (!message || !message->is_dict()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  observer_->OnMessage(message->GetDict());
}

scoped_refptr<base::SingleThreadTaskRunner> ArcSupportMessageHost::task_runner()
    const {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace arc
