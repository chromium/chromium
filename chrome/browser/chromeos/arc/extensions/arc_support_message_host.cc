// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/extensions/arc_support_message_host.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"

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

void ArcSupportMessageHost::SendMessage(const base::Value& message) {
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

  std::unique_ptr<base::Value> message_value =
      base::JSONReader::ReadDeprecated(message_string);
  base::DictionaryValue* message;
  if (!message_value || !message_value->GetAsDictionary(&message)) {
    NOTREACHED();
    return;
  }

  observer_->OnMessage(*message);
}

scoped_refptr<base::SingleThreadTaskRunner> ArcSupportMessageHost::task_runner()
    const {
  return base::ThreadTaskRunnerHandle::Get();
}

}  // namespace arc
