// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_device.h"

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/vr/arcore_device/fake_arcore.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/arcore/arcore_gl.h"
#include "device/vr/android/compositor_delegate_provider.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/android/xr_java_coordinator.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

namespace device {

class StubArImageTransport : public ArImageTransport {
 public:
  explicit StubArImageTransport(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge)
      : ArImageTransport(std::move(mailbox_bridge)),
        shared_buffer_(std::make_unique<WebXrSharedBuffer>()) {}

  void Initialize(WebXrPresentationState*,
                  XrInitStatusCallback callback) override {
    std::move(callback).Run(true);
  }

  // TODO(lincolnfrog): test verify this somehow.
  GLuint GetCameraTextureId() override { return CAMERA_TEXTURE_ID; }

  // This transfers whatever the contents of the texture specified
  // by GetCameraTextureId() is at the time it is called and intends
  // to return to its caller a sync token as well as
  // a scoped_refptr<gpu::ClientSharedImage> with that texture copied
  // to a shared buffer. The two values are currently returned
  // together via a wrapping WebXrSharedBuffer.
  // TODO(crbug.com/40286368): Change the return type to
  // scoped_refptr<gpu::ClientSharedImage> once the sync token is
  // incorporated into ClientSharedImage.
  WebXrSharedBuffer* TransferFrame(
      WebXrPresentationState*,
      const gfx::Size& frame_size,
      const gfx::Transform& uv_transform) override {
    shared_buffer_->shared_image = gpu::ClientSharedImage::CreateForTesting();
    shared_buffer_->sync_token = gpu::SyncToken();
    return shared_buffer_.get();
  }
  WebXrSharedBuffer* TransferCameraImageFrame(
      WebXrPresentationState*,
      const gfx::Size& frame_size,
      const gfx::Transform& uv_transform) override {
    shared_buffer_->shared_image = gpu::ClientSharedImage::CreateForTesting();
    shared_buffer_->sync_token = gpu::SyncToken();
    return shared_buffer_.get();
  }

  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;
  std::unique_ptr<WebXrSharedBuffer> shared_buffer_;
  const GLuint CAMERA_TEXTURE_ID = 10;
};

class StubArImageTransportFactory : public ArImageTransportFactory {
 public:
  ~StubArImageTransportFactory() override = default;

  std::unique_ptr<ArImageTransport> Create(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge) override {
    return std::make_unique<StubArImageTransport>(std::move(mailbox_bridge));
  }
};

class StubMailboxToSurfaceBridge : public webxr::MailboxToSurfaceBridgeImpl {
 public:
  StubMailboxToSurfaceBridge() = default;

  void CreateAndBindContextProvider(base::OnceClosure callback) override {
    callback_ = std::move(callback);
  }

  bool IsConnected() override { return true; }

  const uint32_t TEXTURE_ID = 1;

 private:
  base::OnceClosure callback_;
};

class StubMailboxToSurfaceBridgeFactory : public MailboxToSurfaceBridgeFactory {
 public:
  std::unique_ptr<device::MailboxToSurfaceBridge> Create() const override {
    return std::make_unique<StubMailboxToSurfaceBridge>();
  }
};

class StubCompositorDelegateProvider : public CompositorDelegateProvider {
 public:
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override {
    return base::android::ScopedJavaLocalRef<jobject>();
  }
};

class StubXrJavaCoordinator : public XrJavaCoordinator {
 public:
  StubXrJavaCoordinator() = default;

  void RequestArSession(
      int render_process_id,
      int render_frame_id,
      bool use_overlay,
      bool can_render_dom_content,
      const CompositorDelegateProvider& compositor_delegate_provider,
      SurfaceReadyCallback ready_callback,
      SurfaceTouchCallback touch_callback,
      JavaShutdownCallback destroyed_callback) override {
    // Return arbitrary screen geometry as stand-in for the expected
    // drawing surface. It's not actually a surface, hence the nullptr
    // instead of a WindowAndroid.
    std::move(ready_callback)
        .Run(nullptr, gpu::kNullSurfaceHandle, nullptr,
             display::Display::Rotation::ROTATE_0, {1024, 512});
  }

  void RequestVrSession(
      int render_process_id,
      int render_frame_id,
      const CompositorDelegateProvider& compositor_delegate_provider,
      SurfaceReadyCallback ready_callback,
      SurfaceTouchCallback touch_callback,
      JavaShutdownCallback destroyed_callback,
      XrSessionButtonTouchedCallback button_touched_callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  void EndSession() override {}

  bool EnsureARCoreLoaded() override { return true; }

  base::android::ScopedJavaLocalRef<jobject> GetCurrentActivityContext()
      override {
    JNIEnv* env = base::android::AttachCurrentThread();
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread =
        env->GetStaticMethodID(activityThread, "currentActivityThread",
                               "()Landroid/app/ActivityThread;");
    jobject at =
        env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jmethodID getApplication = env->GetMethodID(
        activityThread, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(at, getApplication);
    return base::android::ScopedJavaLocalRef<jobject>(env, context);
  }

  base::android::ScopedJavaLocalRef<jobject> GetActivityFrom(
      int render_process_id,
      int render_frame_id) override {
    return nullptr;
  }
};

// Note that this must be created and destroyed on the same thread as the mojo
// bindings were originally opened on. If we don't allow UnassociatedUsage of
// the AssociatedReceiver's, we get a DCHECK in the product code that the
// Receiver's still have a pending association. However, it appears that once we
// call EnableUnassociatedUsage, both ends of the pipe must be destroyed on the
// thread that EnableUnassociatedUsage was called on.
class StubCompositorFrameSink
    : public viz::mojom::DisplayPrivate,
      public viz::mojom::CompositorFrameSink,
      public viz::mojom::ExternalBeginFrameController {
 public:
  StubCompositorFrameSink(
      viz::mojom::RootCompositorFrameSinkParamsPtr root_params)
      : sink_client_(std::move(root_params->compositor_frame_sink_client)),
        display_client_(std::move(root_params->display_client)),
        task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
    root_params->compositor_frame_sink.EnableUnassociatedUsage();
    root_params->display_private.EnableUnassociatedUsage();
    root_params->external_begin_frame_controller.EnableUnassociatedUsage();

    frame_sink_.Bind(std::move(root_params->compositor_frame_sink));
    display_private_.Bind(std::move(root_params->display_private));
    frame_controller_.Bind(
        std::move(root_params->external_begin_frame_controller));
  }
  ~StubCompositorFrameSink() override {
    // See class comment for explanation.
    DCHECK(task_runner_->BelongsToCurrentThread());
  }

  // mojom::DisplayPrivate:
  void SetDisplayVisible(bool visible) override {}
  void Resize(const gfx::Size& size) override {}
  void SetDisplayColorMatrix(const gfx::Transform& color_matrix) override {}
  void SetDisplayColorSpaces(
      const gfx::DisplayColorSpaces& display_color_spaces) override {}
  void SetOutputIsSecure(bool secure) override {}
  void SetDisplayVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval) override {}
  void ForceImmediateDrawAndSwapIfPossible() override {}
  void SetVSyncPaused(bool paused) override {}
  void UpdateRefreshRate(float refresh_rate) override {}
  void SetSupportedRefreshRates(
      const std::vector<float>& supported_refresh_rates) override {}
  void PreserveChildSurfaceControls() override {}
  void AddVSyncParameterObserver(
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override {}
  void SetDelegatedInkPointRenderer(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver)
      override {}
  void SetSwapCompletionCallbackEnabled(bool enable) override {}
  void SetStandaloneBeginFrameObserver(
      mojo::PendingRemote<viz::mojom::BeginFrameObserver> observer) override {}
  void SetMaxVSyncAndVrr(std::optional<base::TimeDelta> max_vsync_interval,
                         display::VariableRefreshRateState vrr_state) override {
  }

  // mojom::CompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override {}
  void SetWantsAnimateOnlyBeginFrames() override {}
  void SetWantsBeginFrameAcks() override {}
  void SetAutoNeedsBeginFrame() override {}
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time) override {}
  void DidNotProduceFrame(const viz::BeginFrameAck& begin_frame_ack) override {}
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override {}
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override {}
  void SubmitCompositorFrameSync(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time,
      SubmitCompositorFrameSyncCallback callback) override {}
  void InitializeCompositorFrameSinkType(
      viz::mojom::CompositorFrameSinkType type) override {}
  void BindLayerContext(viz::mojom::PendingLayerContextPtr context) override {}
  void SetThreadIds(const std::vector<int32_t>& thread_ids) override {}

  // mojom::ExternalBeginFrameController implementation.
  void IssueExternalBeginFrame(
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback) override {
    std::move(callback).Run({args, false});
  }

 private:
  mojo::Remote<viz::mojom::CompositorFrameSinkClient> sink_client_;
  mojo::Remote<viz::mojom::DisplayClient> display_client_;
  mojo::AssociatedReceiver<viz::mojom::CompositorFrameSink> frame_sink_{this};
  mojo::AssociatedReceiver<viz::mojom::DisplayPrivate> display_private_{this};
  mojo::AssociatedReceiver<viz::mojom::ExternalBeginFrameController>
      frame_controller_{this};

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class StubXrFrameSinkClient : public XrFrameSinkClient {
 public:
  StubXrFrameSinkClient() = default;
  ~StubXrFrameSinkClient() override {
    if (mojo_thread_task_runner_) {
      mojo_thread_task_runner_->DeleteSoon(FROM_HERE,
                                           std::move(compositor_frame_sink_));
    }
  }

  // device::XrFrameSinkClient
  void InitializeRootCompositorFrameSink(
      viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
      DomOverlaySetup dom_setup,
      base::OnceClosure on_initialized) override {
    // The StubCompositorFrameSink must be created/destroyed on the same thread
    // as the mojo bindings in RootCompositorFrameSinkParamsPtr were on. Since
    // this call comes from the ArCompositorFrameSink, which only runs on the Gl
    // thread, we know that the mojo bindings were opened on this thread. So,
    // we make this the thread to create/destroy the StubCompositorFrameSink on.
    mojo_thread_task_runner_ =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    compositor_frame_sink_ =
        std::make_unique<StubCompositorFrameSink>(std::move(root_params));
    std::move(on_initialized).Run();
  }
  void SurfaceDestroyed() override {}
  std::optional<viz::SurfaceId> GetDOMSurface() override {
    return std::nullopt;
  }
  viz::FrameSinkId FrameSinkId() override { return {}; }

 private:
  std::unique_ptr<StubCompositorFrameSink> compositor_frame_sink_;
  scoped_refptr<base::SingleThreadTaskRunner> mojo_thread_task_runner_;
};

std::unique_ptr<XrFrameSinkClient> FrameSinkClientFactory(int32_t, int32_t) {
  return std::make_unique<StubXrFrameSinkClient>();
}

class ArCoreDeviceTest : public testing::Test {
 public:
  ArCoreDeviceTest() {}
  ~ArCoreDeviceTest() override {}

  void OnSessionCreated(mojom::XRRuntimeSessionResultPtr session_result) {
    DVLOG(1) << __func__;
    session_ = std::move(session_result->session);
    controller_.Bind(std::move(session_result->controller));
    // TODO(crbug.com/41386002): verify that things fail if restricted.
    // We should think through the right result here for javascript.
    // If an AR page tries to hittest while not focused, should it
    // get no results or fail?
    controller_->SetFrameDataRestricted(false);

    frame_provider.Bind(std::move(session_->data_provider));
    frame_provider->GetEnvironmentIntegrationProvider(
        environment_provider.BindNewEndpointAndPassReceiver());
    std::move(quit_closure).Run();
  }

  raw_ptr<StubXrJavaCoordinator> session_utils;
  mojo::Remote<mojom::XRFrameDataProvider> frame_provider;
  mojo::AssociatedRemote<mojom::XREnvironmentIntegrationProvider>
      environment_provider;
  std::unique_ptr<base::RunLoop> run_loop;
  base::OnceClosure quit_closure;

 protected:
  void SetUp() override {
    std::unique_ptr<StubXrJavaCoordinator> session_utils_ptr =
        std::make_unique<StubXrJavaCoordinator>();
    session_utils = session_utils_ptr.get();
    device_ = std::make_unique<ArCoreDevice>(
        std::make_unique<FakeArCoreFactory>(),
        std::make_unique<StubArImageTransportFactory>(),
        std::make_unique<StubMailboxToSurfaceBridgeFactory>(),
        std::move(session_utils_ptr),
        std::make_unique<StubCompositorDelegateProvider>(),
        base::BindRepeating(&FrameSinkClientFactory));
  }

  void CreateSession() {
    mojom::XRRuntimeSessionOptionsPtr options =
        mojom::XRRuntimeSessionOptions::New();
    options->mode = mojom::XRSessionMode::kImmersiveAr;
    device()->RequestSession(std::move(options),
                             base::BindOnce(&ArCoreDeviceTest::OnSessionCreated,
                                            base::Unretained(this)));

    // TODO(crbug.com/41386002): figure out how to make this work
    // EXPECT_CALL(*bridge,
    // DoCreateUnboundContextProvider(testing::_)).Times(1);

    run_loop = std::make_unique<base::RunLoop>();
    quit_closure = run_loop->QuitClosure();
    run_loop->Run();

    EXPECT_TRUE(environment_provider);
    EXPECT_TRUE(session_);
  }

  mojom::XRFrameDataPtr GetFrameData() {
    run_loop = std::make_unique<base::RunLoop>();
    quit_closure = run_loop->QuitClosure();

    mojom::XRFrameDataPtr frame_data;
    auto callback = [](base::OnceClosure run_loop_quit_closure,
                       mojom::XRFrameDataPtr* frame_data,
                       mojom::XRFrameDataPtr data) {
      *frame_data = std::move(data);
      std::move(run_loop_quit_closure).Run();
    };

    // TODO(crbug.com/41386002): verify GetFrameData fails if we
    // haven't resolved the Mailbox.
    frame_provider->GetFrameData(
        nullptr,
        base::BindOnce(callback, std::move(quit_closure), &frame_data));
    run_loop->Run();
    EXPECT_TRUE(frame_data);

    return frame_data;
  }

  VRDeviceBase* device() { return device_.get(); }

 private:
  std::unique_ptr<ArCoreDevice> device_;
  mojom::XRSessionPtr session_;
  mojo::Remote<mojom::XRSessionController> controller_;
};

TEST_F(ArCoreDeviceTest, RequestSession) {
  CreateSession();
}

TEST_F(ArCoreDeviceTest, GetFrameData) {
  CreateSession();
  GetFrameData();
}

}  // namespace device
