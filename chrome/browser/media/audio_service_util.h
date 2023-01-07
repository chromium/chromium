// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_AUDIO_SERVICE_UTIL_H_
#define CHROME_BROWSER_MEDIA_AUDIO_SERVICE_UTIL_H_

#include "build/build_config.h"

bool IsAudioServiceSandboxEnabled();

#if BUILDFLAG(IS_WIN)
bool IsAudioProcessHighPriorityEnabled();
#endif

#endif  // CHROME_BROWSER_MEDIA_AUDIO_SERVICE_UTIL_H_
