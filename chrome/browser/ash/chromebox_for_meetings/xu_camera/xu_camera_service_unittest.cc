// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"

#include <asm-generic/errno.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>

#include <cstdint>
#include <optional>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/xu_camera.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

using chromeos::IpPeripheralServiceClient;

namespace ash::cfm {

// Calls XuCameraService's handlers directly, bypassing mojo. Mojo will not
// send (DCHECK) or dispatch (validation error) a null value for a non-nullable
// parameter, so the service's defensive null checks are only reachable this
// way.
class XuCameraServiceTestPeer {
 public:
  static void GetDevicePath(
      mojom::WebcamIdPtr id,
      base::OnceCallback<void(const std::optional<std::string>&)> callback) {
    XuCameraService::Get()->GetDevicePath(
        std::move(id), content::GlobalRenderFrameHostId(), std::move(callback));
  }

  static void MapCtrlWithDevicePath(mojom::ControlMappingPtr mapping_ctrl,
                                    XuCameraService::MapCtrlCallback callback,
                                    const std::string& dev_path) {
    XuCameraService::Get()->MapCtrlWithDevicePath(
        std::move(mapping_ctrl), std::move(callback), dev_path);
  }

  static void GetCtrlWithDevicePath(mojom::CtrlTypePtr ctrl,
                                    mojom::GetFn fn,
                                    XuCameraService::GetCtrlCallback callback,
                                    const std::string& dev_path) {
    XuCameraService::Get()->GetCtrlWithDevicePath(
        std::move(ctrl), fn, std::move(callback), dev_path);
  }

  static void SetCtrlWithDevicePath(mojom::CtrlTypePtr ctrl,
                                    const std::vector<uint8_t>& data,
                                    XuCameraService::SetCtrlCallback callback,
                                    const std::string& dev_path) {
    XuCameraService::Get()->SetCtrlWithDevicePath(
        std::move(ctrl), data, std::move(callback), dev_path);
  }
};

namespace {

const std::vector<uint8_t> kGuid() {
  return std::vector<uint8_t>({0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34,
                               0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34});
}
auto kMenuEntries() {
  return mojom::MenuEntries::New();
}
const std::vector<uint8_t> kEmpty() {
  return std::vector<uint8_t>();
}
const std::vector<uint8_t> kName() {
  return std::vector<uint8_t>(32, 'a');
}
const std::vector<uint8_t> kData() {
  return std::vector<uint8_t>({0x43, 0x21});
}
const std::vector<uint8_t> kLen() {
  return std::vector<uint8_t>({0x02, 0x00});
}  // little-endian uint16
const int32_t kValue = 123;  // Fake v4l2 value
const std::vector<uint8_t> kValueAsUint8() {
  return std::vector<uint8_t>(
      (std::uint8_t*)&(kValue),
      UNSAFE_TODO((std::uint8_t*)&(kValue) + sizeof(std::int32_t)));
}

mojom::WebcamIdPtr kDevPath() {
  return mojom::WebcamId::NewDevPath("/dev/video0");
}
mojom::WebcamIdPtr kInvalidDevPath() {
  return mojom::WebcamId::NewDevPath("/dev/null");
}
mojom::WebcamIdPtr kInvalidTraversalPath() {
  return mojom::WebcamId::NewDevPath("/dev/video/../dri/card0");
}
mojom::WebcamIdPtr kInvalidIpAddr() {
  return mojom::WebcamId::NewDevPath("192.168.1.300");
}
mojom::WebcamIdPtr kIPAddr() {
  return mojom::WebcamId::NewDevPath("192.168.19.224");
}
mojom::WebcamIdPtr kDevId() {
  return mojom::WebcamId::NewDeviceId("123");
}
mojom::CtrlTypePtr kQueryCtrl() {
  return mojom::CtrlType::NewQueryCtrl(mojom::ControlQuery::New(1, 1));
}
mojom::CtrlTypePtr kCtrlMapping() {
  return mojom::CtrlType::NewMappingCtrl(mojom::ControlMapping::New(
      /* id= */ 1,
      /* name= */ kName(),
      /* guid= */ kGuid(),
      /* selector= */ 1,
      /* size= */ 1,
      /* offset= */ 1,
      /* v4l2_type= */ V4L2_CTRL_TYPE_INTEGER,
      /* data_type= */ UVC_CTRL_DATA_TYPE_SIGNED,
      /* menu_entries= */ kMenuEntries()->Clone()));
}

class TestDelegate : public XuCameraService::Delegate {
 public:
  int Ioctl(const base::ScopedFD& fd,
            unsigned int request,
            void* query) override {
    if (VIDIOC_G_CTRL == request) {
      struct v4l2_control* control = static_cast<v4l2_control*>(query);
      control->value = kValue;
    } else if (UVCIOC_CTRL_QUERY == request) {
      uvc_xu_control_query* control_query =
          static_cast<uvc_xu_control_query*>(query);
      if (UVC_GET_LEN == control_query->query) {
        control_query->data[0] = kLen()[0];
        UNSAFE_TODO(control_query->data[1]) = kLen()[1];
      } else if (UVC_GET_CUR == control_query->query) {
        control_query->data[0] = kData()[0];
        UNSAFE_TODO(control_query->data[1]) = kData()[1];
      }
    }
    return 0;
  }

  bool OpenFile(base::ScopedFD& fd, const std::string& path) override {
    if (!IsValidV4L2DevicePath(path)) {
      LOG(ERROR) << "Filepath is invalid: " << path;
      return false;
    }
    // Return fake fd for unit tests.
    fd = base::ScopedFD(dup(STDERR_FILENO));
    return true;
  }
};

class CfMXuCameraServiceTest
    : public testing::TestWithParam<struct XuTestCase> {
 public:
  CfMXuCameraServiceTest() = default;
  CfMXuCameraServiceTest(const CfMXuCameraServiceTest&) = delete;
  CfMXuCameraServiceTest& operator=(const CfMXuCameraServiceTest&) = delete;

  void SetUp() override {
    IpPeripheralServiceClient::InitializeFake();

    CfmHotlineClient::InitializeFake();
    chromeos::cfm::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    XuCameraService::InitializeForTesting(new TestDelegate());
  }

  void TearDown() override {
    XuCameraService::Shutdown();
    CfmHotlineClient::Shutdown();
    IpPeripheralServiceClient::Shutdown();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  // Returns a mojo::Remote for the mojom::XuCamera by faking the
  // way the cfm mojom binder daemon would request it through chrome.
  const mojo::Remote<mojom::XuCamera>& GetXuCameraRemote() {
    if (!XuCameraService::IsInitialized()) {
      XuCameraService::InitializeForTesting(&delegate_);
    }
    if (xu_camera_remote_.is_bound()) {
      return xu_camera_remote_;
    }

    // if there is no valid remote create one
    auto* interface_name = mojom::XuCamera::Name_;

    base::RunLoop run_loop;

    // Fake out CfmServiceContext
    fake_service_connection_.SetCallback(base::BindLambdaForTesting(
        [&](mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
                pending_receiver,
            bool success) {
          ASSERT_TRUE(success);
          context_receiver_set_.Add(&context_, std::move(pending_receiver));
        }));

    context_.SetFakeProvideAdaptorCallback(base::BindLambdaForTesting(
        [&](const std::string& service_id,
            mojo::PendingRemote<chromeos::cfm::mojom::CfmServiceAdaptor>
                pending_adaptor_remote,
            chromeos::cfm::mojom::CfmServiceContext::ProvideAdaptorCallback
                callback) {
          EXPECT_EQ(interface_name, service_id);
          adaptor_remote_.Bind(std::move(pending_adaptor_remote));
          std::move(callback).Run(true);
          run_loop.Quit();
        }));

    const bool signal_emitted = GetClient()->FakeEmitSignal(interface_name);
    EXPECT_TRUE(signal_emitted);
    if (signal_emitted) {
      run_loop.Run();
    }

    EXPECT_TRUE(adaptor_remote_.is_connected());

    adaptor_remote_->OnBindService(
        xu_camera_remote_.BindNewPipeAndPassReceiver().PassPipe());
    EXPECT_TRUE(xu_camera_remote_.is_connected());

    return xu_camera_remote_;
  }

 protected:
  chromeos::cfm::FakeCfmServiceContext context_;
  mojo::Remote<mojom::XuCamera> xu_camera_remote_;
  mojo::ReceiverSet<chromeos::cfm::mojom::CfmServiceContext>
      context_receiver_set_;
  mojo::Remote<chromeos::cfm::mojom::CfmServiceAdaptor> adaptor_remote_;
  chromeos::cfm::FakeServiceConnectionImpl fake_service_connection_;
  content::BrowserTaskEnvironment task_environment_;
  TestDelegate delegate_;
};

// This test ensures that the XuCameraService is discoverable by its
// mojom name by sending a signal received by CfmHotlineClient.
TEST_F(CfMXuCameraServiceTest, XuCameraServiceAvailable) {
  ASSERT_TRUE(GetClient()->FakeEmitSignal(mojom::XuCamera::Name_));
}

// This test ensures that the XuCameraService correctly registers itself
// for discovery by the cfm mojom binder daemon and correctly returns a
// working mojom remote.
TEST_F(CfMXuCameraServiceTest, GetXuCameraRemote) {
  ASSERT_TRUE(GetXuCameraRemote().is_connected());
}

// This test ensure that the XU camera can get unit id
TEST_F(CfMXuCameraServiceTest, GetXuCameraUnitId) {
  base::RunLoop run_loop;
  GetXuCameraRemote()->GetUnitId(
      /* id= */ kDevPath().Clone(), /* guid= */ kGuid(),
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const uint8_t unit_id) {
            EXPECT_EQ(error_code, ENOSYS);
            EXPECT_EQ(unit_id, '0');
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(CfMXuCameraServiceTest, GetXuCameraUnitIdIPCamera) {
  base::RunLoop run_loop;
  GetXuCameraRemote()->GetUnitId(
      /* id= */ kIPAddr().Clone(), /* guid= */ kGuid(),
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const uint8_t unit_id) {
            EXPECT_EQ(error_code, 0);
            EXPECT_EQ(unit_id, 0);
            run_loop.Quit();
          }));
  run_loop.Run();
}

// This test ensure that the XU camera can map control
TEST_F(CfMXuCameraServiceTest, GetXuCameraMapCtrl) {
  base::RunLoop run_loop;
  GetXuCameraRemote()->MapCtrl(
      /* id= */ kDevPath().Clone(),
      /* mapping_ctrl= */ kCtrlMapping()->get_mapping_ctrl().Clone(),
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, 0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test ensures that map control is rejected for invalid device paths.
TEST_F(CfMXuCameraServiceTest, GetXuCameraMapCtrlInvalidPath) {
  base::RunLoop run_loop;
  GetXuCameraRemote()->MapCtrl(
      /* id= */ kInvalidDevPath().Clone(),
      /* mapping_ctrl= */ kCtrlMapping()->get_mapping_ctrl().Clone(),
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, ENOENT);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test ensures that map control is rejected for IP camera paths.
TEST_F(CfMXuCameraServiceTest, GetXuCameraMapCtrlIPCamera) {
  base::RunLoop run_loop;
  GetXuCameraRemote()->MapCtrl(
      /* id= */ kIPAddr().Clone(),
      /* mapping_ctrl= */ kCtrlMapping()->get_mapping_ctrl().Clone(),
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, ENOENT);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test ensures that map control handles null menu_entries safely.
TEST_F(CfMXuCameraServiceTest, GetXuCameraMapCtrlNullMenuEntries) {
  base::RunLoop run_loop;
  auto ctrl_mapping = mojom::ControlMapping::New(
      /* id= */ 1,
      /* name= */ kName(),
      /* guid= */ kGuid(),
      /* selector= */ 1,
      /* size= */ 1,
      /* offset= */ 1,
      /* v4l2_type= */ V4L2_CTRL_TYPE_INTEGER,
      /* data_type= */ UVC_CTRL_DATA_TYPE_SIGNED,
      /* menu_entries= */ nullptr);
  GetXuCameraRemote()->MapCtrl(
      /* id= */ kDevPath().Clone(),
      /* mapping_ctrl= */ std::move(ctrl_mapping),
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, 0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test ensures that map control is rejected for null mapping control.
TEST_F(CfMXuCameraServiceTest, GetXuCameraMapCtrlNullMapping) {
  base::RunLoop run_loop;
  XuCameraServiceTestPeer::MapCtrlWithDevicePath(
      /* mapping_ctrl= */ nullptr,
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, EINVAL);
        run_loop.Quit();
      }),
      /* dev_path= */ "/dev/video0");
  run_loop.Run();
}

// This test ensures that get control is rejected for null control parameter.
TEST_F(CfMXuCameraServiceTest, GetXuCameraGetCtrlNullCtrl) {
  base::RunLoop run_loop;
  XuCameraServiceTestPeer::GetCtrlWithDevicePath(
      /* ctrl= */ nullptr,
      /* fn= */ mojom::GetFn::kCur,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const std::vector<uint8_t>& data) {
            EXPECT_EQ(error_code, EINVAL);
            EXPECT_TRUE(data.empty());
            run_loop.Quit();
          }),
      /* dev_path= */ "/dev/video0");
  run_loop.Run();
}

// This test ensures that set control is rejected for null control parameter.
TEST_F(CfMXuCameraServiceTest, GetXuCameraSetCtrlNullCtrl) {
  base::RunLoop run_loop;
  XuCameraServiceTestPeer::SetCtrlWithDevicePath(
      /* ctrl= */ nullptr,
      /* data= */ {'a', 'b', 'c'},
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, EINVAL);
        run_loop.Quit();
      }),
      /* dev_path= */ "/dev/video0");
  run_loop.Run();
}

// This test ensures that a null webcam id resolves to no device path.
TEST_F(CfMXuCameraServiceTest, GetXuCameraDevicePathNullId) {
  base::RunLoop run_loop;
  XuCameraServiceTestPeer::GetDevicePath(
      /* id= */ nullptr,
      base::BindLambdaForTesting([&](const std::optional<std::string>& path) {
        EXPECT_FALSE(path.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test validates device path and IP peripheral address format checking.
TEST_F(CfMXuCameraServiceTest, DevicePathValidation) {
  // Valid local V4L2 device paths.
  EXPECT_TRUE(IsValidV4L2DevicePath("/dev/video0"));
  EXPECT_TRUE(IsValidV4L2DevicePath("/dev/video1"));
  EXPECT_TRUE(IsValidV4L2DevicePath("/dev/video11"));
  EXPECT_TRUE(IsValidV4L2DevicePath("/dev/video128"));
  EXPECT_TRUE(IsValidV4L2DevicePath("/dev/video5566"));
  EXPECT_TRUE(IsValidV4L2DevicePath("/dev/video99999"));
  EXPECT_TRUE(IsValidDevPath("/dev/video0"));

  // Out of range index or invalid suffix.
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video100000"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/videox"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video0a"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video0/something"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video0/../../etc/shadow"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video/../dri/card0"));

  // Leading zeros on multi-digit video device indices.
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video00"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video01"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/video007"));

  // Non-video device paths and paths outside /dev.
  EXPECT_FALSE(IsValidV4L2DevicePath(""));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/null"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/dri/card0"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/tmp/sample_target"));
  EXPECT_FALSE(IsValidV4L2DevicePath("../../../../../../tmp/sample_target"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/dev/input/event0"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/proc/self/status"));
  EXPECT_FALSE(IsValidV4L2DevicePath("/sys/class/leds/x/brightness"));
  EXPECT_FALSE(IsValidDevPath("/dev/null"));

  // Valid IP camera addresses.
  EXPECT_TRUE(IsIpCamera("192.168.0.1"));
  EXPECT_TRUE(IsIpCamera("192.168.19.224"));
  EXPECT_TRUE(IsIpCamera("192.168.0.0"));
  EXPECT_TRUE(IsIpCamera("192.168.255.255"));
  EXPECT_TRUE(IsValidDevPath("192.168.19.224"));

  // Invalid IP camera addresses.
  EXPECT_FALSE(IsIpCamera(""));
  EXPECT_FALSE(IsIpCamera("192.168."));
  EXPECT_FALSE(IsIpCamera("192.168.1"));
  EXPECT_FALSE(IsIpCamera("192.168.1.1.1"));
  EXPECT_FALSE(IsIpCamera("192.168.1.256"));
  EXPECT_FALSE(IsIpCamera("192.168.1.300"));
  EXPECT_FALSE(IsIpCamera("192.168.1.foo"));
  EXPECT_FALSE(IsIpCamera("192.168.1.1/../../etc/passwd"));
  EXPECT_FALSE(IsIpCamera("192.168.1.1:8080"));
  EXPECT_FALSE(IsIpCamera("10.0.0.1"));
  EXPECT_FALSE(IsIpCamera("192.168.01.1"));
  EXPECT_FALSE(IsIpCamera("192.168.1.01"));
  EXPECT_FALSE(IsIpCamera("192.168.00.00"));
  EXPECT_FALSE(IsIpCamera("192.168.0.01"));
  EXPECT_FALSE(IsIpCamera("192.168.0.00"));
  EXPECT_FALSE(IsValidDevPath("192.168.1.300"));
}

// This test ensure that the XU camera can get control length given a ctrl
// query
TEST_F(CfMXuCameraServiceTest, XuCameraGetCtrlLenWithDevPathCtrlQuery) {
  base::RunLoop run_loop;
  GetXuCameraRemote()->GetCtrl(
      /* id= */ kDevPath().Clone(), /* ctrl= */ kQueryCtrl().Clone(),
      /* fn= */ mojom::GetFn::kLen,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const std::vector<uint8_t>& data) {
            EXPECT_EQ(error_code, 0);
            EXPECT_EQ(data, kLen());
            run_loop.Quit();
          }));
  run_loop.Run();
}

struct XuTestCase {
  XuTestCase(std::string test_name,
             mojom::WebcamIdPtr webcam_id,
             mojom::CtrlTypePtr ctrl_type,
             uint8_t expected_error_code,
             std::vector<uint8_t> expected_data)
      : test_name(test_name),
        webcam_id(std::move(webcam_id)),
        ctrl_type(std::move(ctrl_type)),
        expected_error_code(expected_error_code),
        expected_data(expected_data) {}
  XuTestCase(const XuTestCase& other)
      : test_name(other.test_name),
        webcam_id(other.webcam_id.Clone()),
        ctrl_type(other.ctrl_type.Clone()),
        expected_error_code(other.expected_error_code),
        expected_data(other.expected_data) {}
  std::string test_name;
  const mojom::WebcamIdPtr webcam_id;
  const mojom::CtrlTypePtr ctrl_type;
  uint8_t expected_error_code;
  std::vector<uint8_t> expected_data;  // used only for GetCtrl tests
};

std::vector<XuTestCase> GetXuTestCases() {
  return {
      {"DevPath_CtrlQuery", kDevPath(), kQueryCtrl(), 0, kData()},
      {"DevId_CtrlQuery", kDevId(), kQueryCtrl(), ENOENT, kEmpty()},
      {"DevPath_CtrlMapping", kDevPath(), kCtrlMapping(), 0, kValueAsUint8()},
      {"DevId_CtrlMapping", kDevId(), kCtrlMapping(), ENOENT, kEmpty()},
      {"IPAddr_CtrlQuery", kIPAddr(), kQueryCtrl(), 0, kEmpty()},
      {"IPAddr_CtrlMapping", kIPAddr(), kCtrlMapping(), ENOENT, kEmpty()},
      {"InvalidDevPath_CtrlQuery", kInvalidDevPath(), kQueryCtrl(), ENOENT,
       kEmpty()},
      {"InvalidDevPath_CtrlMapping", kInvalidDevPath(), kCtrlMapping(), ENOENT,
       kEmpty()},
      {"InvalidTraversal_CtrlQuery", kInvalidTraversalPath(), kQueryCtrl(),
       ENOENT, kEmpty()},
      {"InvalidIpAddr_CtrlQuery", kInvalidIpAddr(), kQueryCtrl(), ENOENT,
       kEmpty()},
  };
}

// Test that the XU camera can get control given a ctrl query/mapping
TEST_P(CfMXuCameraServiceTest, XuCameraGetCtrl) {
  base::RunLoop run_loop;
  const XuTestCase& param = GetParam();
  GetXuCameraRemote()->GetCtrl(
      /* id= */ param.webcam_id.Clone(), /* ctrl= */ param.ctrl_type.Clone(),
      /* fn= */ mojom::GetFn::kCur,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const std::vector<uint8_t>& data) {
            EXPECT_EQ(error_code, param.expected_error_code);
            EXPECT_EQ(data, param.expected_data);
            run_loop.Quit();
          }));
  run_loop.Run();
}

// Test that the XU camera can set control given a ctrl query/mapping
TEST_P(CfMXuCameraServiceTest, XuCameraSetCtrl) {
  base::RunLoop run_loop;
  const XuTestCase& param = GetParam();
  std::vector<uint8_t> data{'a', 'b', 'c'};
  GetXuCameraRemote()->SetCtrl(
      /* id= */ param.webcam_id.Clone(), /* ctrl= */ param.ctrl_type.Clone(),
      /* data= */ data,
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, param.expected_error_code);
        run_loop.Quit();
      }));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    CfMXuCameraServiceTests,
    CfMXuCameraServiceTest,
    testing::ValuesIn(GetXuTestCases()),
    [](const testing::TestParamInfo<CfMXuCameraServiceTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash::cfm
