// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump.h"

#include "base/check.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include "base/message_loop/message_pump_mac.h"
#endif

namespace base {

namespace {

MessagePump::MessagePumpFactory* message_pump_for_ui_factory_ = nullptr;

}  // namespace

MessagePump::MessagePump() = default;

MessagePump::~MessagePump() = default;

void MessagePump::SetTimerSlack(TimerSlack) {
}

// static
void MessagePump::OverrideMessagePumpForUIFactory(MessagePumpFactory* factory) {
  DCHECK(!message_pump_for_ui_factory_);
  message_pump_for_ui_factory_ = factory;
}

// static
bool MessagePump::IsMessagePumpForUIFactoryOveridden() {
  return message_pump_for_ui_factory_ != nullptr;
}

// static
std::unique_ptr<MessagePump> MessagePump::Create(MessagePumpType type) {
  switch (type) {
    case MessagePumpType::UI:
      if (message_pump_for_ui_factory_)
        return message_pump_for_ui_factory_();
#if BUILDFLAG(IS_APPLE)
      return MessagePumpMac::Create();
#elif BUILDFLAG(IS_NACL) || BUILDFLAG(IS_AIX)
      // Currently NaCl and AIX don't have a UI MessagePump.
      // TODO(abarth): Figure out if we need this.
      NOTREACHED();
      return nullptr;
#else
      return std::make_unique<MessagePumpForUI>();
#endif

    case MessagePumpType::IO:
      return std::make_unique<MessagePumpForIO>();

#if BUILDFLAG(IS_ANDROID)
    case MessagePumpType::JAVA:
      return std::make_unique<MessagePumpForUI>();
#endif

#if BUILDFLAG(IS_APPLE)
    case MessagePumpType::NS_RUNLOOP:
      return std::make_unique<MessagePumpNSRunLoop>();
#endif

    case MessagePumpType::CUSTOM:
      NOTREACHED();
      return nullptr;

    case MessagePumpType::DEFAULT:
#if BUILDFLAG(IS_IOS)
      // On iOS, a native runloop is always required to pump system work.
      return std::make_unique<MessagePumpCFRunLoop>();
#else
      return std::make_unique<MessagePumpDefault>();
#endif
  }
}

}  // namespace base
