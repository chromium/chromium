// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace gl {
class GLSurface;
}

namespace gpu {
class GLInProcessContext;
namespace gles2 {
class GLES2TraceImplementation;
}  // namespace gles2
}  // namespace gpu

namespace android_webview {

class AwRenderThreadContextProvider
    : public base::RefCountedThreadSafe<AwRenderThreadContextProvider>,
      public viz::ContextProvider {
 public:
  static scoped_refptr<AwRenderThreadContextProvider> Create(
      scoped_refptr<gl::GLSurface> surface,
      gpu::CommandBufferTaskExecutor* task_executor);

  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // on the default framebuffer.
  uint32_t GetCopyTextureInternalFormat();

  // viz::ContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentThread() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  viz::ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  void AddObserver(viz::ContextLostObserver* obs) override;
  void RemoveObserver(viz::ContextLostObserver* obs) override;

 protected:
  friend class base::RefCountedThreadSafe<AwRenderThreadContextProvider>;

  AwRenderThreadContextProvider(scoped_refptr<gl::GLSurface> surface,
                                gpu::CommandBufferTaskExecutor* task_executor);
  ~AwRenderThreadContextProvider() override;

 private:
  void OnLostContext();

  base::ThreadChecker main_thread_checker_;

  std::unique_ptr<gpu::GLInProcessContext> context_;
  std::unique_ptr<gpu::gles2::GLES2TraceImplementation> trace_impl_;
  sk_sp<class GrContext> gr_context_;
  std::unique_ptr<viz::ContextCacheController> cache_controller_;

  base::ObserverList<viz::ContextLostObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(AwRenderThreadContextProvider);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_
