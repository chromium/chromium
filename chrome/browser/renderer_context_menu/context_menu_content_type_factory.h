// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_FACTORY_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_FACTORY_H_

#include <memory>

#include "content/public/browser/context_menu_params.h"

class ContextMenuContentType;

namespace content {
class RenderFrameHost;
}

class ContextMenuContentTypeFactory {
 public:
  static std::unique_ptr<ContextMenuContentType> Create(
      content::RenderFrameHost* render_frame_host,
      const content::ContextMenuParams& params);

  ContextMenuContentTypeFactory(const ContextMenuContentTypeFactory&) = delete;
  ContextMenuContentTypeFactory& operator=(
      const ContextMenuContentTypeFactory&) = delete;

 private:
  ContextMenuContentTypeFactory();
  virtual ~ContextMenuContentTypeFactory();

  static std::unique_ptr<ContextMenuContentType> CreateInternal(
      content::RenderFrameHost* render_frame_host,
      const content::ContextMenuParams& params);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_FACTORY_H_
