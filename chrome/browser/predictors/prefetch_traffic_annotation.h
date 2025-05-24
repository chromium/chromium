// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREFETCH_TRAFFIC_ANNOTATION_H_
#define CHROME_BROWSER_PREDICTORS_PREFETCH_TRAFFIC_ANNOTATION_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace predictors {

// The traffic annotation used for prefetches by the PrefetchManager class and
// PerformNetworkContextPrefetch() function. The definition of this variable is
// in prefetch_manager.cc to avoid confusing the traffic annotation auditor.
extern const net::NetworkTrafficAnnotationTag kPrefetchTrafficAnnotation;

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREFETCH_TRAFFIC_ANNOTATION_H_
