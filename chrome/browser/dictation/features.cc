// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/features.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace dictation {

BASE_FEATURE(kDictation, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDictationEvalMode{&kDictation, "eval_mode",
                                                  false};

const base::FeatureParam<bool> kUseComponentExtension{
    &kDictation, "use_component_extension", true};

const base::FeatureParam<bool> kSendContextAsync{&kDictation,
                                                 "send_context_async", false};

const base::FeatureParam<bool> kShowPartials{&kDictation, "show_partials",
                                             false};

const base::FeatureParam<bool> kWebSpeechApiBackend{
    &kDictation, "web_speech_api_backend", false};

const base::FeatureParam<bool> kSessionEndsOnStreamEnd{
    &kDictation, "session_ends_on_stream_end", true};

const base::FeatureParam<base::TimeDelta> kAutoSessionEndDelay{
    &kDictation, "auto_session_end_delay", base::Milliseconds(750)};

}  // namespace dictation
