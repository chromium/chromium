// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/mojom/constants.mojom.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/base64url.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/fake_recommend_apps_fetcher_delegate.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/arc/arc_features_parser.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace chromeos {

namespace {

// Values set in ArcFeatures created by CreateArcFeaturesForTest.
constexpr char kTestArcSdkVersion[] = "25";
constexpr char kTestArcPlayStoreVersion[] = "81010860";
const char* const kTestArcAbiList[] = {"x86", "x86_64"};
const char* const kTestArcFeatures[] = {"android.hardware.faketouch",
                                        "android.software.home_screen"};

// Creates a fake ARC features to be used for these tests.
arc::ArcFeatures CreateArcFeaturesForTest() {
  arc::ArcFeatures arc_features;
  arc_features.build_props["ro.build.version.sdk"] = kTestArcSdkVersion;
  arc_features.play_store_version = kTestArcPlayStoreVersion;

  std::vector<std::string> abi_list(std::begin(kTestArcAbiList),
                                    std::end(kTestArcAbiList));
  arc_features.build_props["ro.product.cpu.abilist"] =
      base::JoinString(abi_list, ",");

  for (const char* feature : kTestArcFeatures) {
    arc_features.feature_map[feature] = 1;
  }
  return arc_features;
}

class TestCrosDisplayConfig : public ash::mojom::CrosDisplayConfigController,
                              public service_manager::Service {
 public:
  explicit TestCrosDisplayConfig(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}
  ~TestCrosDisplayConfig() override = default;

  void Flush() {
    if (!ready_) {
      base::RunLoop run_loop;
      ready_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    receiver_.FlushForTesting();
  }

  bool RunGetDisplayUnitInfoListCallback(
      std::vector<ash::mojom::DisplayUnitInfoPtr> unit_info_list) {
    if (!get_display_unit_info_list_callback_)
      return false;
    std::move(get_display_unit_info_list_callback_)
        .Run(std::move(unit_info_list));
    return true;
  }

  // ash::mojom::CrosDisplayConfigController:
  void AddObserver(
      mojo::PendingAssociatedRemote<ash::mojom::CrosDisplayConfigObserver>
          observer) override {}
  void GetDisplayLayoutInfo(GetDisplayLayoutInfoCallback callback) override {}
  void SetDisplayLayoutInfo(ash::mojom::DisplayLayoutInfoPtr info,
                            SetDisplayLayoutInfoCallback callback) override {}
  void GetDisplayUnitInfoList(
      bool single_unified,
      GetDisplayUnitInfoListCallback callback) override {
    get_display_unit_info_list_callback_ = std::move(callback);
  }
  void SetDisplayProperties(const std::string& id,
                            ash::mojom::DisplayConfigPropertiesPtr properties,
                            ash::mojom::DisplayConfigSource source,
                            SetDisplayPropertiesCallback callback) override {}
  void SetUnifiedDesktopEnabled(bool enabled) override {}
  void OverscanCalibration(const std::string& display_id,
                           ash::mojom::DisplayConfigOperation op,
                           const base::Optional<gfx::Insets>& delta,
                           OverscanCalibrationCallback callback) override {}
  void TouchCalibration(const std::string& display_id,
                        ash::mojom::DisplayConfigOperation op,
                        ash::mojom::TouchCalibrationPtr calibration,
                        TouchCalibrationCallback callback) override {}

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    DCHECK(interface_name == ash::mojom::CrosDisplayConfigController::Name_);
    receiver_.Bind(
        mojo::PendingReceiver<ash::mojom::CrosDisplayConfigController>(
            std::move(interface_pipe)));
    ready_ = true;
    if (ready_callback_)
      std::move(ready_callback_).Run();
  }

 private:
  service_manager::ServiceBinding service_binding_;
  mojo::Receiver<ash::mojom::CrosDisplayConfigController> receiver_{this};

  bool ready_ = false;
  base::OnceClosure ready_callback_;
  GetDisplayUnitInfoListCallback get_display_unit_info_list_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestCrosDisplayConfig);
};

// Helper class to extract relevant information from the app list request
// headers.
class AppListRequestHeaderReader {
 public:
  explicit AppListRequestHeaderReader(network::ResourceRequest* request) {
    request->headers.GetHeader("X-DFE-Sdk-Version", &sdk_version_);
    request->headers.GetHeader("X-DFE-Chromesky-Client-Version",
                               &play_store_version_);
    DecodeDeviceConfigHeader(request);
  }
  ~AppListRequestHeaderReader() = default;

  AppListRequestHeaderReader(const AppListRequestHeaderReader& other) = delete;
  AppListRequestHeaderReader& operator=(
      const AppListRequestHeaderReader& other) = delete;

  const std::string& sdk_version() const { return sdk_version_; }

  const std::string& play_store_version() const { return play_store_version_; }

  device_configuration::DeviceConfigurationProto_TouchScreen touch_screen()
      const {
    return device_config_.touch_screen();
  }

  int screen_density() const { return device_config_.screen_density(); }

  int screen_height() const { return device_config_.screen_height(); }

  int screen_width() const { return device_config_.screen_width(); }

  device_configuration::DeviceConfigurationProto_ScreenLayout screen_layout()
      const {
    return device_config_.screen_layout();
  }

  device_configuration::DeviceConfigurationProto_Keyboard keyboard() const {
    return device_config_.keyboard();
  }

  bool has_hard_keyboard() const { return device_config_.has_hard_keyboard(); }

  std::vector<std::string> GetNativePlatforms() const {
    std::vector<std::string> result;
    for (int i = 0; i < device_config_.native_platform_size(); ++i) {
      result.push_back(device_config_.native_platform(i));
    }
    std::sort(result.begin(), result.end());
    return result;
  }

  std::vector<std::string> GetSystemAvailableFeatures() const {
    std::vector<std::string> result;
    for (int i = 0; i < device_config_.system_available_feature_size(); ++i) {
      result.push_back(device_config_.system_available_feature(i));
    }
    std::sort(result.begin(), result.end());
    return result;
  }

 private:
  void DecodeDeviceConfigHeader(network::ResourceRequest* request) {
    std::string device_config_header;
    ASSERT_TRUE(request->headers.GetHeader("X-DFE-Device-Config",
                                           &device_config_header));

    std::string decoded;
    ASSERT_TRUE(Base64UrlDecode(device_config_header,
                                base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                                &decoded));
    std::string decompressed;
    ASSERT_TRUE(compression::GzipUncompress(decoded, &decompressed));

    ASSERT_TRUE(device_config_.ParseFromString(decompressed));
  }

  std::string sdk_version_;
  std::string play_store_version_;
  device_configuration::DeviceConfigurationProto device_config_;
};

}  // namespace

class RecommendAppsFetcherImplTest : public testing::Test {
 public:
  RecommendAppsFetcherImplTest() = default;
  ~RecommendAppsFetcherImplTest() override = default;

  void SetUp() override {
    display::Screen::SetScreenInstance(&test_screen_);
    display::Display::SetInternalDisplayId(
        test_screen_.GetPrimaryDisplay().id());

    cros_display_config_ = std::make_unique<TestCrosDisplayConfig>(
        connector_factory_.RegisterInstance(ash::mojom::kServiceName));

    test_url_loader_factory_.SetInterceptor(
        base::BindRepeating(&RecommendAppsFetcherImplTest::InterceptRequest,
                            base::Unretained(this)));

    recommend_apps_fetcher_ = std::make_unique<RecommendAppsFetcherImpl>(
        &delegate_, connector_factory_.GetDefaultConnector(),
        &test_url_loader_factory_);

    static_cast<RecommendAppsFetcherImpl*>(recommend_apps_fetcher_.get())
        ->set_arc_features_getter_for_testing(base::BindRepeating(
            &RecommendAppsFetcherImplTest::HandleArcFeaturesRequest,
            base::Unretained(this)));
  }

  void TearDown() override {
    recommend_apps_fetcher_.reset();
    cros_display_config_.reset();
    display::Screen::SetScreenInstance(nullptr);
    device_data_manager_test_api_.SetKeyboardDevices({});
    device_data_manager_test_api_.SetTouchscreenDevices({});
  }

 protected:
  struct Dpi {
    Dpi(float x, float y) : x(x), y(y) {}
    ~Dpi() = default;

    const float x;
    const float y;
  };

  std::vector<ash::mojom::DisplayUnitInfoPtr> CreateDisplayUnitInfo(
      const Dpi& internal_dpi,
      base::Optional<Dpi> external_dpi) {
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list;

    if (external_dpi.has_value()) {
      auto external_info = ash::mojom::DisplayUnitInfo::New();
      external_info->id =
          base::NumberToString(test_screen_.GetPrimaryDisplay().id() + 1);
      external_info->is_internal = false;
      external_info->dpi_x = external_dpi->x;
      external_info->dpi_y = external_dpi->y;
      info_list.emplace_back(std::move(external_info));
    }

    auto info = ash::mojom::DisplayUnitInfo::New();
    info->id = base::NumberToString(test_screen_.GetPrimaryDisplay().id());
    info->is_internal = true;
    info->dpi_x = internal_dpi.x;
    info->dpi_y = internal_dpi.y;
    info_list.emplace_back(std::move(info));

    return info_list;
  }

  network::ResourceRequest* WaitForAppListRequest() {
    if (test_url_loader_factory_.pending_requests()->size() == 0) {
      request_waiter_ = std::make_unique<base::RunLoop>();
      request_waiter_->Run();
      request_waiter_.reset();
    }
    return &test_url_loader_factory_.GetPendingRequest(0)->request;
  }

  void SetDisplaySize(const gfx::Size& size) {
    display::Display display = test_screen_.GetPrimaryDisplay();
    display.SetSize(size);
    test_screen_.display_list().RemoveDisplay(display.id());
    test_screen_.display_list().AddDisplay(display,
                                           display::DisplayList::Type::PRIMARY);
  }

  void VerifyArcRequestHeaders(
      const AppListRequestHeaderReader& header_reader) {
    EXPECT_EQ(kTestArcSdkVersion, header_reader.sdk_version());
    EXPECT_EQ(kTestArcPlayStoreVersion, header_reader.play_store_version());
    EXPECT_EQ(std::vector<std::string>(std::begin(kTestArcAbiList),
                                       std::end(kTestArcAbiList)),
              header_reader.GetNativePlatforms());
    EXPECT_EQ(std::vector<std::string>(std::begin(kTestArcFeatures),
                                       std::end(kTestArcFeatures)),
              header_reader.GetSystemAvailableFeatures());
  }

  FakeRecommendAppsFetcherDelegate delegate_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<RecommendAppsFetcher> recommend_apps_fetcher_;
  std::unique_ptr<TestCrosDisplayConfig> cros_display_config_;

  ui::DeviceDataManagerTestApi device_data_manager_test_api_;
  display::test::TestScreen test_screen_;
  base::OnceCallback<void(base::Optional<arc::ArcFeatures>)>
      arc_features_callback_;

 private:
  void InterceptRequest(const network::ResourceRequest& request) {
    ASSERT_EQ(
        "https://android.clients.google.com/fdfe/chrome/"
        "getfastreinstallappslist",
        request.url.spec());
    if (request_waiter_)
      request_waiter_->Quit();
  }

  void HandleArcFeaturesRequest(
      base::OnceCallback<void(base::Optional<arc::ArcFeatures>)> callback) {
    arc_features_callback_ = std::move(callback);
  }

  content::BrowserTaskEnvironment task_environment_;

  service_manager::TestConnectorFactory connector_factory_;

  std::unique_ptr<base::RunLoop> request_waiter_;
};

TEST_F(RecommendAppsFetcherImplTest, ExtraLargeScreenWithTouch) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetTouchscreenDevices({ui::TouchscreenDevice(
      123, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("test external touch device"), gfx::Size(1920, 1200), 1)});
  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"}});
  SetDisplaySize(gfx::Size(1920, 1200));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(110, 120), Dpi(117.23, 117.23))));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  VerifyArcRequestHeaders(header_reader);

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_FINGER,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_QWERTY,
            header_reader.keyboard());
  EXPECT_FALSE(header_reader.has_hard_keyboard());
  EXPECT_EQ(230, header_reader.screen_density());
  EXPECT_EQ(1920, header_reader.screen_width());
  EXPECT_EQ(1200, header_reader.screen_height());
  EXPECT_EQ(
      device_configuration::DeviceConfigurationProto_ScreenLayout_EXTRA_LARGE,
      header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());

  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, NoArcFeatures) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetTouchscreenDevices({ui::TouchscreenDevice(
      123, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("test external touch device"), gfx::Size(1920, 1200), 1)});
  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"}});
  SetDisplaySize(gfx::Size(1920, 1200));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(110, 120), Dpi(117.23, 117.23))));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(base::nullopt);

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  EXPECT_EQ("", header_reader.sdk_version());
  EXPECT_EQ("", header_reader.play_store_version());
  EXPECT_EQ(std::vector<std::string>(), header_reader.GetNativePlatforms());
  EXPECT_EQ(std::vector<std::string>(),
            header_reader.GetSystemAvailableFeatures());

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_FINGER,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_QWERTY,
            header_reader.keyboard());
  EXPECT_FALSE(header_reader.has_hard_keyboard());
  EXPECT_EQ(230, header_reader.screen_density());
  EXPECT_EQ(1920, header_reader.screen_width());
  EXPECT_EQ(1200, header_reader.screen_height());
  EXPECT_EQ(
      device_configuration::DeviceConfigurationProto_ScreenLayout_EXTRA_LARGE,
      header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());

  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, HasHardKeyboard) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetTouchscreenDevices({ui::TouchscreenDevice(
      123, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("test external touch device"), gfx::Size(1920, 1200), 1)});
  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard", "phys",
       base::FilePath("sys_path"), 0, 0, 0}});
  SetDisplaySize(gfx::Size(1920, 1200));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(110, 120), Dpi(117.23, 117.23))));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  VerifyArcRequestHeaders(header_reader);

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_FINGER,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_QWERTY,
            header_reader.keyboard());
  EXPECT_TRUE(header_reader.has_hard_keyboard());
  EXPECT_EQ(230, header_reader.screen_density());
  EXPECT_EQ(1920, header_reader.screen_width());
  EXPECT_EQ(1200, header_reader.screen_height());
  EXPECT_EQ(
      device_configuration::DeviceConfigurationProto_ScreenLayout_EXTRA_LARGE,
      header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());

  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, NoKeyboard) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  SetDisplaySize(gfx::Size(1920, 1200));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(110, 120), Dpi(117.23, 117.23))));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  VerifyArcRequestHeaders(header_reader);

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_NOTOUCH,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_NOKEYS,
            header_reader.keyboard());
  EXPECT_FALSE(header_reader.has_hard_keyboard());
  EXPECT_EQ(230, header_reader.screen_density());
  EXPECT_EQ(1920, header_reader.screen_width());
  EXPECT_EQ(1200, header_reader.screen_height());
  EXPECT_EQ(
      device_configuration::DeviceConfigurationProto_ScreenLayout_EXTRA_LARGE,
      header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());

  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, ExtraLargeScreenWithStylus) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetTouchscreenDevices(
      {ui::TouchscreenDevice(123, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                             std::string("test external touch device"),
                             gfx::Size(1200, 1920), 1, true)});
  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"}});

  SetDisplaySize(gfx::Size(1200, 1920));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117.23, 117.23), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  VerifyArcRequestHeaders(header_reader);

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_STYLUS,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_QWERTY,
            header_reader.keyboard());
  EXPECT_FALSE(header_reader.has_hard_keyboard());
  EXPECT_EQ(234, header_reader.screen_density());
  EXPECT_EQ(1200, header_reader.screen_width());
  EXPECT_EQ(1920, header_reader.screen_height());
  EXPECT_EQ(
      device_configuration::DeviceConfigurationProto_ScreenLayout_EXTRA_LARGE,
      header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, LargeScreenWithoutTouchScreen) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"}});

  SetDisplaySize(gfx::Size(1200, 1200));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  VerifyArcRequestHeaders(header_reader);

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_NOTOUCH,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_QWERTY,
            header_reader.keyboard());
  EXPECT_FALSE(header_reader.has_hard_keyboard());
  EXPECT_EQ(234, header_reader.screen_density());
  EXPECT_EQ(1200, header_reader.screen_width());
  EXPECT_EQ(1200, header_reader.screen_height());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_ScreenLayout_LARGE,
            header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, NormalScreenWithoutTouchScreen) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"}});

  SetDisplaySize(gfx::Size(1200, 512));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  VerifyArcRequestHeaders(header_reader);

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_NOTOUCH,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_QWERTY,
            header_reader.keyboard());
  EXPECT_FALSE(header_reader.has_hard_keyboard());
  EXPECT_EQ(234, header_reader.screen_density());
  EXPECT_EQ(1200, header_reader.screen_width());
  EXPECT_EQ(512, header_reader.screen_height());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_ScreenLayout_NORMAL,
            header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, SmallScreenWithoutTouchScreen) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
      {2, ui::INPUT_DEVICE_USB, "external keyboard"}});

  SetDisplaySize(gfx::Size(512, 456));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  VerifyArcRequestHeaders(header_reader);

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_NOTOUCH,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_QWERTY,
            header_reader.keyboard());
  EXPECT_FALSE(header_reader.has_hard_keyboard());
  EXPECT_EQ(234, header_reader.screen_density());
  EXPECT_EQ(512, header_reader.screen_width());
  EXPECT_EQ(456, header_reader.screen_height());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_ScreenLayout_SMALL,
            header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, ArcFeaturesReadyBeforeAsh) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
      {2, ui::INPUT_DEVICE_USB, "external keyboard"}});

  SetDisplaySize(gfx::Size(512, 456));

  recommend_apps_fetcher_->Start();

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  AppListRequestHeaderReader header_reader(request);

  VerifyArcRequestHeaders(header_reader);

  EXPECT_EQ(device_configuration::DeviceConfigurationProto_TouchScreen_NOTOUCH,
            header_reader.touch_screen());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_Keyboard_QWERTY,
            header_reader.keyboard());
  EXPECT_FALSE(header_reader.has_hard_keyboard());
  EXPECT_EQ(234, header_reader.screen_density());
  EXPECT_EQ(512, header_reader.screen_width());
  EXPECT_EQ(456, header_reader.screen_height());
  EXPECT_EQ(device_configuration::DeviceConfigurationProto_ScreenLayout_SMALL,
            header_reader.screen_layout());

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, RetryCalledBeforeFirstRequest) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  device_data_manager_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
      {2, ui::INPUT_DEVICE_USB, "external keyboard"}});

  SetDisplaySize(gfx::Size(512, 456));

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  recommend_apps_fetcher_->Retry();
  EXPECT_TRUE(test_url_loader_factory_.pending_requests()->empty());

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());

  EXPECT_TRUE(test_url_loader_factory_.pending_requests()->empty());
}

TEST_F(RecommendAppsFetcherImplTest, EmptyResponse) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  SetDisplaySize(gfx::Size(512, 456));

  recommend_apps_fetcher_->Start();

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);
  test_url_loader_factory_.AddResponse(request->url.spec(), "");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::LOAD_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, EmptyAppList) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  SetDisplaySize(gfx::Size(512, 456));

  recommend_apps_fetcher_->Start();

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);
  test_url_loader_factory_.AddResponse(request->url.spec(), "[]");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, ResponseWithLeadeingBrackets) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  const std::string response =
      R"()]}'[{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, MalformedJsonResponse) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(), ")}]'!2%^$");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, UnexpectedResponseType) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(), "\"abcd\"");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, ResponseWithMultipleApps) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }, {
           "id_": {"id_": "test.app2"}
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app1(base::Value::Type::DICTIONARY);
  app1.SetKey("name", base::Value("Test app 1"));
  app1.SetKey("icon", base::Value("http://test.app"));
  app1.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app1));

  base::Value app2(base::Value::Type::DICTIONARY);
  app2.SetKey("package_name", base::Value("test.app2"));
  expected_apps.Append(std::move(app2));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, InvalidAppItemsIgnored) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }, [], 2, {"id_": {"id_": "test.app2"}}, {"a": "b"}])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app1(base::Value::Type::DICTIONARY);
  app1.SetKey("name", base::Value("Test app 1"));
  app1.SetKey("icon", base::Value("http://test.app"));
  app1.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app1));

  base::Value app2(base::Value::Type::DICTIONARY);
  app2.SetKey("package_name", base::Value("test.app2"));
  expected_apps.Append(std::move(app2));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, DictionaryResponse) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(), "{}");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, InvalidErrorCodeType) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(),
                                       R"({"Error code": ""})");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, ResponseWithErrorCode) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(),
                                       R"({"Error code": "6"})");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, NotEnoughAppsError) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(),
                                       R"({"Error code": "5"})");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, AppListRequestFailure) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(), "",
                                       net::HTTP_BAD_REQUEST);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::LOAD_ERROR,
            delegate_.WaitForResult());
}

TEST_F(RecommendAppsFetcherImplTest, SuccessOnRetry) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(),
                                       R"({"Error code": "5"})");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
  delegate_.Reset();
  test_url_loader_factory_.ClearResponses();

  recommend_apps_fetcher_->Retry();

  request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  const std::string response =
      R"()]}'[{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());

  base::Value expected_apps(base::Value::Type::LIST);
  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey("name", base::Value("Test app 1"));
  app.SetKey("icon", base::Value("http://test.app"));
  app.SetKey("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(RecommendAppsFetcherImplTest, FailureOnRetry) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->Start();

  cros_display_config_->Flush();
  ASSERT_TRUE(cros_display_config_->RunGetDisplayUnitInfoListCallback(
      CreateDisplayUnitInfo(Dpi(117, 117), base::nullopt)));

  ASSERT_TRUE(arc_features_callback_);
  std::move(arc_features_callback_).Run(CreateArcFeaturesForTest());

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);
  test_url_loader_factory_.AddResponse(request->url.spec(),
                                       R"({"Error code": "5"})");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
  delegate_.Reset();
  test_url_loader_factory_.ClearResponses();

  recommend_apps_fetcher_->Retry();

  request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(),
                                       R"({"Error code": "10"})");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

}  // namespace chromeos
