// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_UI_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_UI_H_

// This header is a forwarding header to coalesce the various platform
// specific implementations of MessagePumpForUI.
// 此头文件是一个转发头文件，用于合并 MessagePumpForUI 的各种平台特定实现。

// 这里的跨平台手法值得学习：
// 有这么一种情况，.h文件中的接口也不能跨平台，那么就需要各平台有自己的头文件，即需
// 要下面代码中的这种方式来统一各平台的对外类名，这样使用方可以直接include这个头文
// 件，各平台也能统一使用同一个类名：MessagePumpForUI

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/message_loop/message_pump_win.h"
#elif defined(OS_ANDROID)
#include "base/message_loop/message_pump_android.h"
#elif defined(OS_APPLE)
#include "base/message_loop/message_pump.h"
#elif defined(OS_NACL) || defined(OS_AIX)
// No MessagePumpForUI, see below.
#elif defined(USE_GLIB)
#include "base/message_loop/message_pump_glib.h"
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_BSD)
#include "base/message_loop/message_pump_libevent.h"
#elif defined(OS_FUCHSIA)
#include "base/message_loop/message_pump_fuchsia.h"
#endif

namespace base {

#if defined(OS_WIN)
// Windows defines it as-is.
using MessagePumpForUI = MessagePumpForUI;
#elif defined(OS_ANDROID)
// Android defines it as-is.
using MessagePumpForUI = MessagePumpForUI;
#elif defined(OS_APPLE)
// MessagePumpForUI isn't bound to a specific impl on Mac. While each impl can
// be represented by a plain MessagePump: MessagePumpMac::Create() must be used
// to instantiate the right impl.
using MessagePumpForUI = MessagePump;
#elif defined(OS_NACL) || defined(OS_AIX)
// Currently NaCl and AIX don't have a MessagePumpForUI.
// TODO(abarth): Figure out if we need this.
#elif defined(USE_GLIB)
using MessagePumpForUI = MessagePumpGlib;
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_BSD)
using MessagePumpForUI = MessagePumpLibevent;
#elif defined(OS_FUCHSIA)
using MessagePumpForUI = MessagePumpFuchsia;
#else
#error Platform does not define MessagePumpForUI
#endif

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_UI_H_
