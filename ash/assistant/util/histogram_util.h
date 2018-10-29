// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_
#define ASH_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_

namespace ash {

enum class AssistantSource;

namespace assistant {
namespace util {

// Increment number of queries fired for each entry point.
void IncrementAssistantQueryCountForEntryPoint(AssistantSource entry_point);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_
