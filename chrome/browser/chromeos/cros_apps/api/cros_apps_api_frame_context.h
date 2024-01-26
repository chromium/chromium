// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_FRAME_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_FRAME_CONTEXT_H_

#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class NavigationHandle;
}  // namespace content

class Profile;

// Encapsulates information required to make a decision on whether an API should
// be enabled in a frame running ChromeOS App.
//
// Convertible from RenderFrameHost and NavigationHandle.
class CrosAppsApiFrameContext {
  // This class should be used as a temporary. Mark as STACK_ALLOCATED to reduce
  // the risk of `this` outlives the referenced `context_`, i.e. RenderFrameHost
  // and Navigation Handle.
  STACK_ALLOCATED();

 public:
  explicit CrosAppsApiFrameContext(content::RenderFrameHost& rfh);
  explicit CrosAppsApiFrameContext(
      content::NavigationHandle& navigation_handle);

  CrosAppsApiFrameContext(const CrosAppsApiFrameContext&) = delete;
  CrosAppsApiFrameContext& operator=(const CrosAppsApiFrameContext&) = delete;
  CrosAppsApiFrameContext(CrosAppsApiFrameContext&&) = delete;
  CrosAppsApiFrameContext& operator=(const CrosAppsApiFrameContext&&) = delete;

  ~CrosAppsApiFrameContext();

  // URL of the document.
  const GURL& GetUrl() const;

  // Whether this frame is the primary main frame.
  bool IsPrimaryMainFrame() const;

  // Returns the Profile.
  const Profile* Profile() const;

 private:
  const absl::variant<raw_ref<content::RenderFrameHost>,
                      raw_ref<content::NavigationHandle>>
      context_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_FRAME_CONTEXT_H_
