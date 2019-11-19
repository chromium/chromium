// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/internal/pending_response.h>
#include <lib/fidl/cpp/internal/weak_stub_controller.h>
#include <lib/zx/debuglog.h>
#include <zircon/syscalls/log.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "build/fuchsia/fidlgen_js/fidl/fidljstest/cpp/fidl.h"
#include "build/fuchsia/fidlgen_js/runtime/zircon.h"
#include "gin/converter.h"
#include "gin/modules/console.h"
#include "gin/object_template_builder.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "v8/include/v8.h"

static const char kRuntimeFile[] =
    "/pkg/build/fuchsia/fidlgen_js/runtime/fidl.mjs";
static const char kTestBindingFile[] =
    "/pkg/build/fuchsia/fidlgen_js/fidl/fidljstest/js/fidl.js";

namespace {

zx_koid_t GetKoidForHandle(zx_handle_t handle) {
  zx_info_handle_basic_t info;
  zx_status_t status = zx_object_get_info(handle, ZX_INFO_HANDLE_BASIC, &info,
                                          sizeof(info), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info";
    return ZX_KOID_INVALID;
  }
  return info.koid;
}

zx_koid_t GetKoidForHandle(const zx::object_base& object) {
  return GetKoidForHandle(object.get());
}

}  // namespace

class FidlGenJsTestShellRunnerDelegate : public gin::ShellRunnerDelegate {
 public:
  FidlGenJsTestShellRunnerDelegate() {}

  v8::Local<v8::ObjectTemplate> GetGlobalTemplate(
      gin::ShellRunner* runner,
      v8::Isolate* isolate) override {
    v8::Local<v8::ObjectTemplate> templ =
        gin::ObjectTemplateBuilder(isolate).Build();
    gin::Console::Register(isolate, templ);
    return templ;
  }

  void UnhandledException(gin::ShellRunner* runner,
                          gin::TryCatch& try_catch) override {
    LOG(ERROR) << try_catch.GetStackTrace();
    ADD_FAILURE();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FidlGenJsTestShellRunnerDelegate);
};

using FidlGenJsTest = gin::V8Test;

TEST_F(FidlGenJsTest, DISABLED_BasicJSSetup) {
  v8::Isolate* isolate = instance_->isolate();

  std::string source = "log('this is a log'); this.stuff = 'HAI';";
  FidlGenJsTestShellRunnerDelegate delegate;
  gin::ShellRunner runner(&delegate, isolate);
  gin::Runner::Scope scope(&runner);
  runner.Run(source, "test.js");

  std::string result;
  EXPECT_TRUE(gin::Converter<std::string>::FromV8(
      isolate,
      runner.global()
          ->Get(isolate->GetCurrentContext(), gin::StringToV8(isolate, "stuff"))
          .ToLocalChecked(),
      &result));
  EXPECT_EQ("HAI", result);
}

void LoadAndSource(gin::ShellRunner* runner, const base::FilePath& filename) {
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(filename, &contents));

  runner->Run(contents, filename.MaybeAsASCII());
}

class BindingsSetupHelper {
 public:
  explicit BindingsSetupHelper(v8::Isolate* isolate)
      : isolate_(isolate),
        handle_scope_(isolate),
        delegate_(),
        runner_(&delegate_, isolate),
        scope_(&runner_),
        zx_bindings_(
            std::make_unique<fidljs::ZxBindings>(isolate, runner_.global())) {
    // TODO(scottmg): Figure out how to set up v8 import hooking and make
    // fidl_Xyz into $fidl.Xyz. Manually inject the runtime support js files
    // for now. https://crbug.com/883496.
    LoadAndSource(&runner_, base::FilePath(kRuntimeFile));
    LoadAndSource(&runner_, base::FilePath(kTestBindingFile));

    zx_status_t status = zx::channel::create(0, &server_, &client_);
    EXPECT_EQ(status, ZX_OK);

    runner_.global()
        ->Set(isolate->GetCurrentContext(),
              gin::StringToSymbol(isolate, "testHandle"),
              gin::ConvertToV8(isolate, client_.get()))
        .Check();
  }

  template <class T>
  T Get(const std::string& name) {
    T t;
    EXPECT_TRUE(
        gin::Converter<T>::FromV8(isolate_,
                                  runner_.global()
                                      ->Get(isolate_->GetCurrentContext(),
                                            gin::StringToV8(isolate_, name))
                                      .ToLocalChecked(),
                                  &t));
    return t;
  }

  template <class T>
  T FromV8BigInt(v8::Local<v8::Value> val);

  template <>
  uint64_t FromV8BigInt(v8::Local<v8::Value> val) {
    EXPECT_TRUE(val->IsBigInt());
    return val.As<v8::BigInt>()->Uint64Value(nullptr);
  }

  template <>
  int64_t FromV8BigInt(v8::Local<v8::Value> val) {
    EXPECT_TRUE(val->IsBigInt());
    return val.As<v8::BigInt>()->Int64Value(nullptr);
  }

  // Custom version of gin::Converter that handles int64/uint64 from BigInt as
  // gin::Converter is quite tied to Number.
  template <class T>
  std::vector<T> GetBigIntVector(const std::string& name) {
    v8::Local<v8::Context> context = isolate_->GetCurrentContext();
    v8::Local<v8::Value> val =
        runner_.global()
            ->Get(context, gin::StringToV8(isolate_, name))
            .ToLocalChecked();
    EXPECT_TRUE(val->IsArray());

    std::vector<T> result;
    v8::Local<v8::Array> array(v8::Local<v8::Array>::Cast(val));
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; ++i) {
      v8::Local<v8::Value> v8_item;
      EXPECT_TRUE(array->Get(context, i).ToLocal(&v8_item));
      T item;
      if (v8_item->IsNumber()) {
        EXPECT_TRUE(gin::Converter<T>::FromV8(isolate_, v8_item, &item));
      } else if (v8_item->IsBigInt()) {
        item = FromV8BigInt<T>(v8_item);
      } else {
        ADD_FAILURE();
      }
      result.push_back(item);
    }

    return result;
  }

  bool IsNull(const std::string& name) {
    return runner_.global()
        ->Get(isolate_->GetCurrentContext(), gin::StringToV8(isolate_, name))
        .ToLocalChecked()
        ->IsNull();
  }

  void DestroyBindingsForTesting() { zx_bindings_.reset(); }

  zx::channel& server() { return server_; }
  zx::channel& client() { return client_; }
  gin::ShellRunner& runner() { return runner_; }

 private:
  v8::Isolate* isolate_;
  v8::HandleScope handle_scope_;
  FidlGenJsTestShellRunnerDelegate delegate_;
  gin::ShellRunner runner_;
  gin::Runner::Scope scope_;
  std::unique_ptr<fidljs::ZxBindings> zx_bindings_;
  zx::channel server_;
  zx::channel client_;

  DISALLOW_COPY_AND_ASSIGN(BindingsSetupHelper);
};

class AnotherInterfaceImpl : public fidljstest::AnotherInterface {
 public:
  AnotherInterfaceImpl(
      fidl::InterfaceRequest<fidljstest::AnotherInterface> request)
      : binding_(this, std::move(request)) {}
  ~AnotherInterfaceImpl() override = default;

  void TimesTwo(int32_t a, TimesTwoCallback callback) override {
    callback(a * 2);
  }

 private:
  fidl::Binding<fidljstest::AnotherInterface> binding_;

  DISALLOW_COPY_AND_ASSIGN(AnotherInterfaceImpl);
};

class TestolaImpl : public fidljstest::Testola {
 public:
  TestolaImpl() {
    // Don't want the default values from the C++ side.
    memset(&basic_struct_, -1, sizeof(basic_struct_));
  }
  ~TestolaImpl() override {}

  void DoSomething() override { was_do_something_called_ = true; }

  void PrintInt(int32_t number) override { received_int_ = number; }

  void PrintMsg(std::string message) override { received_msg_ = message; }

  void VariousArgs(fidljstest::Blorp blorp,
                   std::string msg,
                   std::vector<uint32_t> stuff) override {
    various_blorp_ = blorp;
    various_msg_ = msg;
    various_stuff_ = stuff;
  }

  void WithResponse(int32_t a,
                    int32_t b,
                    WithResponseCallback callback) override {
    response_callbacks_.push_back(base::BindOnce(
        [](WithResponseCallback callback, int32_t result) { callback(result); },
        std::move(callback), a + b));
  }

  void SendAStruct(fidljstest::BasicStruct basic_struct) override {
    basic_struct_ = basic_struct;
  }

  void NestedStructsWithResponse(
      fidljstest::BasicStruct basic_struct,
      NestedStructsWithResponseCallback resp) override {
    // Construct a response, echoing the passed in structure with some
    // modifications, as well as additional data.
    fidljstest::StuffAndThings sat;
    sat.count = 123;
    sat.id = "here is my id";
    sat.a_vector.push_back(1);
    sat.a_vector.push_back(-2);
    sat.a_vector.push_back(4);
    sat.a_vector.push_back(-8);
    sat.basic.b = !basic_struct.b;
    sat.basic.i8 = basic_struct.i8 * 2;
    sat.basic.i16 = basic_struct.i16 * 2;
    sat.basic.i32 = basic_struct.i32 * 2;
    sat.basic.u8 = basic_struct.u8 * 2;
    sat.basic.u16 = basic_struct.u16 * 2;
    sat.basic.u32 = basic_struct.u32 * 2;
    sat.later_string = "ⓣⓔⓡⓜⓘⓝⓐⓣⓞⓡ";
    for (uint64_t i = 0; i < fidljstest::ARRRR_SIZE; ++i) {
      sat.arrrr[i] = static_cast<int32_t>(i * 5) - 10;
    }
    sat.nullable_vector_of_string0.reset();
    std::vector<std::string> vector_of_str;
    vector_of_str.push_back("passed_str0");
    vector_of_str.push_back("passed_str1");
    sat.nullable_vector_of_string1 = std::move(vector_of_str);
    std::vector<fidljstest::Blorp> vector_of_blorp;
    vector_of_blorp.push_back(fidljstest::Blorp::GAMMA);
    vector_of_blorp.push_back(fidljstest::Blorp::BETA);
    vector_of_blorp.push_back(fidljstest::Blorp::BETA);
    vector_of_blorp.push_back(fidljstest::Blorp::ALPHA);
    sat.vector_of_blorp = std::move(vector_of_blorp);

    resp(std::move(sat));
  }

  void PassHandles(zx::job job, PassHandlesCallback callback) override {
    EXPECT_EQ(GetKoidForHandle(job), GetKoidForHandle(*zx::job::default_job()));
    zx::process process;
    ASSERT_EQ(zx::process::self()->duplicate(ZX_RIGHT_SAME_RIGHTS, &process),
              ZX_OK);
    callback(std::move(process));
  }

  void ReceiveUnions(fidljstest::StructOfMultipleUnions somu) override {
    EXPECT_TRUE(somu.initial.is_swb());
    EXPECT_TRUE(somu.initial.swb().some_bool);

    EXPECT_TRUE(somu.optional.get());
    EXPECT_TRUE(somu.optional->is_lswa());
    for (int i = 0; i < 32; ++i) {
      EXPECT_EQ(somu.optional->lswa().components[i], i * 99);
    }

    EXPECT_TRUE(somu.trailing.is_swu());
    EXPECT_EQ(somu.trailing.swu().num, 123456u);

    did_receive_union_ = true;
  }

  void SendUnions(SendUnionsCallback callback) override {
    fidljstest::StructOfMultipleUnions resp;

    resp.initial.set_swb(fidljstest::StructWithBool());
    resp.initial.swb().some_bool = true;

    resp.optional = std::make_unique<fidljstest::UnionOfStructs>();
    resp.optional->set_swu(fidljstest::StructWithUint());
    resp.optional->swu().num = 987654;

    resp.trailing.set_lswa(fidljstest::LargerStructWithArray());

    callback(std::move(resp));
  }

  void SendVectorsOfString(std::vector<std::string> unsized,
                           std::vector<fidl::StringPtr> nullable,
                           std::vector<std::string> max_strlen) override {
    ASSERT_EQ(unsized.size(), 3u);
    EXPECT_EQ(unsized[0], "str0");
    EXPECT_EQ(unsized[1], "str1");
    EXPECT_EQ(unsized[2], "str2");

    ASSERT_EQ(nullable.size(), 5u);
    EXPECT_EQ(nullable[0], "str3");
    EXPECT_FALSE(nullable[1].has_value());
    EXPECT_FALSE(nullable[2].has_value());
    EXPECT_FALSE(nullable[3].has_value());
    EXPECT_EQ(nullable[4], "str4");

    ASSERT_EQ(max_strlen.size(), 1u);
    EXPECT_EQ(max_strlen[0], "0123456789");

    did_get_vectors_of_string_ = true;
  }

  void VectorOfStruct(std::vector<fidljstest::StructWithUint> stuff,
                      VectorOfStructCallback callback) override {
    ASSERT_EQ(stuff.size(), 4u);
    EXPECT_EQ(stuff[0].num, 456u);
    EXPECT_EQ(stuff[1].num, 789u);
    EXPECT_EQ(stuff[2].num, 123u);
    EXPECT_EQ(stuff[3].num, 0xfffffu);

    std::vector<fidljstest::StructWithUint> response;
    fidljstest::StructWithUint a;
    a.num = 369;
    response.push_back(a);
    fidljstest::StructWithUint b;
    b.num = 258;
    response.push_back(b);
    callback(std::move(response));
  }

  void PassVectorOfPrimitives(
      fidljstest::VectorsOfPrimitives input,
      PassVectorOfPrimitivesCallback callback) override {
    ASSERT_EQ(input.v_bool.size(), 1u);
    ASSERT_EQ(input.v_uint8.size(), 2u);
    ASSERT_EQ(input.v_uint16.size(), 3u);
    ASSERT_EQ(input.v_uint32.size(), 4u);
    ASSERT_EQ(input.v_uint64.size(), 5u);
    ASSERT_EQ(input.v_int8.size(), 6u);
    ASSERT_EQ(input.v_int16.size(), 7u);
    ASSERT_EQ(input.v_int32.size(), 8u);
    ASSERT_EQ(input.v_int64.size(), 9u);
    ASSERT_EQ(input.v_float32.size(), 10u);
    ASSERT_EQ(input.v_float64.size(), 11u);

    EXPECT_EQ(input.v_bool[0], true);

    EXPECT_EQ(input.v_uint8[0], 2u);
    EXPECT_EQ(input.v_uint8[1], 3u);

    EXPECT_EQ(input.v_uint16[0], 4u);
    EXPECT_EQ(input.v_uint16[1], 5u);
    EXPECT_EQ(input.v_uint16[2], 6u);

    EXPECT_EQ(input.v_uint32[0], 7u);
    EXPECT_EQ(input.v_uint32[1], 8u);
    EXPECT_EQ(input.v_uint32[2], 9u);
    EXPECT_EQ(input.v_uint32[3], 10u);

    EXPECT_EQ(input.v_uint64[0], 11u);
    EXPECT_EQ(input.v_uint64[1], 12u);
    EXPECT_EQ(input.v_uint64[2], 13u);
    EXPECT_EQ(input.v_uint64[3], 14u);
    EXPECT_EQ(input.v_uint64[4], 0xffffffffffffff00ULL);

    EXPECT_EQ(input.v_int8[0], -16);
    EXPECT_EQ(input.v_int8[1], -17);
    EXPECT_EQ(input.v_int8[2], -18);
    EXPECT_EQ(input.v_int8[3], -19);
    EXPECT_EQ(input.v_int8[4], -20);
    EXPECT_EQ(input.v_int8[5], -21);

    EXPECT_EQ(input.v_int16[0], -22);
    EXPECT_EQ(input.v_int16[1], -23);
    EXPECT_EQ(input.v_int16[2], -24);
    EXPECT_EQ(input.v_int16[3], -25);
    EXPECT_EQ(input.v_int16[4], -26);
    EXPECT_EQ(input.v_int16[5], -27);
    EXPECT_EQ(input.v_int16[6], -28);

    EXPECT_EQ(input.v_int32[0], -29);
    EXPECT_EQ(input.v_int32[1], -30);
    EXPECT_EQ(input.v_int32[2], -31);
    EXPECT_EQ(input.v_int32[3], -32);
    EXPECT_EQ(input.v_int32[4], -33);
    EXPECT_EQ(input.v_int32[5], -34);
    EXPECT_EQ(input.v_int32[6], -35);
    EXPECT_EQ(input.v_int32[7], -36);

    EXPECT_EQ(input.v_int64[0], -37);
    EXPECT_EQ(input.v_int64[1], -38);
    EXPECT_EQ(input.v_int64[2], -39);
    EXPECT_EQ(input.v_int64[3], -40);
    EXPECT_EQ(input.v_int64[4], -41);
    EXPECT_EQ(input.v_int64[5], -42);
    EXPECT_EQ(input.v_int64[6], -43);
    EXPECT_EQ(input.v_int64[7], -44);
    EXPECT_EQ(input.v_int64[8], -0x7fffffffffffffffLL);

    EXPECT_EQ(input.v_float32[0], 46.f);
    EXPECT_EQ(input.v_float32[1], 47.f);
    EXPECT_EQ(input.v_float32[2], 48.f);
    EXPECT_EQ(input.v_float32[3], 49.f);
    EXPECT_EQ(input.v_float32[4], 50.f);
    EXPECT_EQ(input.v_float32[5], 51.f);
    EXPECT_EQ(input.v_float32[6], 52.f);
    EXPECT_EQ(input.v_float32[7], 53.f);
    EXPECT_EQ(input.v_float32[8], 54.f);
    EXPECT_EQ(input.v_float32[9], 55.f);

    EXPECT_EQ(input.v_float64[0], 56.0);
    EXPECT_EQ(input.v_float64[1], 57.0);
    EXPECT_EQ(input.v_float64[2], 58.0);
    EXPECT_EQ(input.v_float64[3], 59.0);
    EXPECT_EQ(input.v_float64[4], 60.0);
    EXPECT_EQ(input.v_float64[5], 61.0);
    EXPECT_EQ(input.v_float64[6], 62.0);
    EXPECT_EQ(input.v_float64[7], 63.0);
    EXPECT_EQ(input.v_float64[8], 64.0);
    EXPECT_EQ(input.v_float64[9], 65.0);
    EXPECT_EQ(input.v_float64[10], 66.0);

    fidljstest::VectorsOfPrimitives output = std::move(input);
#define INC_OUTPUT_ARRAY(v)                      \
  for (size_t i = 0; i < output.v.size(); ++i) { \
    output.v[i] += 10;                           \
  }
    INC_OUTPUT_ARRAY(v_uint8);
    INC_OUTPUT_ARRAY(v_uint16);
    INC_OUTPUT_ARRAY(v_uint32);
    INC_OUTPUT_ARRAY(v_uint64);
    INC_OUTPUT_ARRAY(v_int8);
    INC_OUTPUT_ARRAY(v_int16);
    INC_OUTPUT_ARRAY(v_int32);
    INC_OUTPUT_ARRAY(v_int64);
    INC_OUTPUT_ARRAY(v_float32);
    INC_OUTPUT_ARRAY(v_float64);
#undef INC_OUTPUT_ARRAY

    callback(std::move(output));
  }

  void PassVectorOfVMO(fidljstest::VectorOfHandleToVMO input,
                       PassVectorOfVMOCallback callback) override {
    callback(std::move(input));
  }

  bool was_do_something_called() const { return was_do_something_called_; }
  int32_t received_int() const { return received_int_; }
  const std::string& received_msg() const { return received_msg_; }

  fidljstest::Blorp various_blorp() const { return various_blorp_; }
  const std::string& various_msg() const { return various_msg_; }
  const std::vector<uint32_t>& various_stuff() const { return various_stuff_; }

  fidljstest::BasicStruct GetReceivedStruct() const { return basic_struct_; }

  bool did_receive_union() const { return did_receive_union_; }

  bool did_get_vectors_of_string() const { return did_get_vectors_of_string_; }

  void CallResponseCallbacks() {
    for (auto& cb : response_callbacks_) {
      std::move(cb).Run();
    }
    response_callbacks_.clear();
  }

  void GetAnother(
      fidl::InterfaceRequest<fidljstest::AnotherInterface> request) override {
    another_interface_impl_ =
        std::make_unique<AnotherInterfaceImpl>(std::move(request));
  }

 private:
  bool was_do_something_called_ = false;
  int32_t received_int_ = -1;
  std::string received_msg_;
  fidljstest::Blorp various_blorp_;
  std::string various_msg_;
  std::vector<uint32_t> various_stuff_;
  fidljstest::BasicStruct basic_struct_;
  std::vector<base::OnceClosure> response_callbacks_;
  bool did_receive_union_ = false;
  bool did_get_vectors_of_string_ = false;
  std::unique_ptr<AnotherInterfaceImpl> another_interface_impl_;

  DISALLOW_COPY_AND_ASSIGN(TestolaImpl);
};

TEST_F(FidlGenJsTest, DISABLED_RawReceiveFidlMessage) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.DoSomething();
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(
      helper.server().read(0, data, handles, base::size(data),
                           base::size(handles), &actual_bytes, &actual_handles),
      ZX_OK);
  EXPECT_EQ(actual_bytes, 16u);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_TRUE(testola_impl.was_do_something_called());
}

TEST_F(FidlGenJsTest, DISABLED_RawReceiveFidlMessageWithSimpleArg) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.PrintInt(12345);
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(
      helper.server().read(0, data, handles, base::size(data),
                           base::size(handles), &actual_bytes, &actual_handles),
      ZX_OK);
  // 24 rather than 20 because everything's 8 aligned.
  EXPECT_EQ(actual_bytes, 24u);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.received_int(), 12345);
}

TEST_F(FidlGenJsTest, DISABLED_RawReceiveFidlMessageWithStringArg) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.PrintMsg('Ça c\'est a 你好 from deep in JS');
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(
      helper.server().read(0, data, handles, base::size(data),
                           base::size(handles), &actual_bytes, &actual_handles),
      ZX_OK);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.received_msg(), "Ça c'est a 你好 from deep in JS");
}

TEST_F(FidlGenJsTest, DISABLED_RawReceiveFidlMessageWithMultipleArgs) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.VariousArgs(Blorp.GAMMA, 'zippy zap', [ 999, 987, 123456 ]);
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(
      helper.server().read(0, data, handles, base::size(data),
                           base::size(handles), &actual_bytes, &actual_handles),
      ZX_OK);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.various_blorp(), fidljstest::Blorp::GAMMA);
  EXPECT_EQ(testola_impl.various_msg(), "zippy zap");
  ASSERT_EQ(testola_impl.various_stuff().size(), 3u);
  EXPECT_EQ(testola_impl.various_stuff()[0], 999u);
  EXPECT_EQ(testola_impl.various_stuff()[1], 987u);
  EXPECT_EQ(testola_impl.various_stuff()[2], 123456u);
}

TEST_F(FidlGenJsTest, DISABLED_RawWithResponse) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  // Send the data from the JS side into the channel.
  std::string source = R"(
      var proxy = new TestolaProxy();
      proxy.$bind(testHandle);
      this.sum_result = -1;
      proxy.WithResponse(72, 99)
           .then(sum => {
              this.sum_result = sum;
            })
           .catch((e) => log('FAILED: ' + e));
    )";
  helper.runner().Run(source, "test.js");

  base::RunLoop().RunUntilIdle();

  testola_impl.CallResponseCallbacks();

  base::RunLoop().RunUntilIdle();

  // Confirm that the response was received with the correct value.
  auto sum_result = helper.Get<int>("sum_result");
  EXPECT_EQ(sum_result, 72 + 99);
}

TEST_F(FidlGenJsTest, DISABLED_NoResponseBeforeTearDown) {
  v8::Isolate* isolate = instance_->isolate();

  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  // Send the data from the JS side into the channel.
  std::string source = R"(
      var proxy = new TestolaProxy();
      proxy.$bind(testHandle);
      this.resolved = false;
      this.rejected = false;
      this.excepted = false;
      proxy.WithResponse(1, 2)
           .then(sum => {
              this.resolved = true;
            }, () => {
              this.rejected = true;
            })
           .catch((e) => {
             log('FAILED: ' + e);
             this.excepted = true;
           })
    )";
  helper.runner().Run(source, "test.js");

  // Run the message loop to read and queue the request, but don't send the
  // response.
  base::RunLoop().RunUntilIdle();

  // This causes outstanding waits to be canceled.
  helper.DestroyBindingsForTesting();

  EXPECT_FALSE(helper.Get<bool>("resolved"));
  EXPECT_TRUE(helper.Get<bool>("rejected"));
  EXPECT_FALSE(helper.Get<bool>("excepted"));
}

TEST_F(FidlGenJsTest, DISABLED_RawReceiveFidlStructMessage) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    var basicStruct = new BasicStruct(
        true, -30, undefined, -789, 200, 65000, 0);
    proxy.SendAStruct(basicStruct);
  )";
  helper.runner().Run(source, "test.js");

  // Run the dispatcher to read and dispatch the response.
  base::RunLoop().RunUntilIdle();

  fidljstest::BasicStruct received_struct = testola_impl.GetReceivedStruct();
  EXPECT_EQ(received_struct.b, true);
  EXPECT_EQ(received_struct.i8, -30);
  EXPECT_EQ(received_struct.i16, 18);  // From defaults.
  EXPECT_EQ(received_struct.i32, -789);
  EXPECT_EQ(received_struct.u8, 200);
  EXPECT_EQ(received_struct.u16, 65000);
  // Make sure this didn't get defaulted, even though it has a false-ish value.
  EXPECT_EQ(received_struct.u32, 0u);
}

TEST_F(FidlGenJsTest, DISABLED_RawReceiveFidlNestedStructsAndRespond) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  // Send the data from the JS side into the channel.
  std::string source = R"(
      var proxy = new TestolaProxy();
      proxy.$bind(testHandle);
      var toSend = new BasicStruct(false, -5, -6, -7, 8, 32000, 2000000000);
      proxy.NestedStructsWithResponse(toSend)
           .then(sat => {
             this.result_count = sat.count;
             this.result_id = sat.id;
             this.result_vector = sat.a_vector;
             this.result_basic_b = sat.basic.b;
             this.result_basic_i8 = sat.basic.i8;
             this.result_basic_i16 = sat.basic.i16;
             this.result_basic_i32 = sat.basic.i32;
             this.result_basic_u8 = sat.basic.u8;
             this.result_basic_u16 = sat.basic.u16;
             this.result_basic_u32 = sat.basic.u32;
             this.result_later_string = sat.later_string;
             this.result_arrrr = sat.arrrr;
             this.result_vs0 = sat.nullable_vector_of_string0;
             this.result_vs1 = sat.nullable_vector_of_string1;
             this.result_vblorp = sat.vector_of_blorp;
           })
           .catch((e) => log('FAILED: ' + e));
    )";
  helper.runner().Run(source, "test.js");

  // Run the message loop to read the request and write the response.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(helper.Get<int>("result_count"), 123);
  EXPECT_EQ(helper.Get<std::string>("result_id"), "here is my id");
  auto result_vector = helper.Get<std::vector<int>>("result_vector");
  ASSERT_EQ(result_vector.size(), 4u);
  EXPECT_EQ(result_vector[0], 1);
  EXPECT_EQ(result_vector[1], -2);
  EXPECT_EQ(result_vector[2], 4);
  EXPECT_EQ(result_vector[3], -8);
  EXPECT_EQ(helper.Get<bool>("result_basic_b"), true);
  EXPECT_EQ(helper.Get<int>("result_basic_i8"), -10);
  EXPECT_EQ(helper.Get<int>("result_basic_i16"), -12);
  EXPECT_EQ(helper.Get<int>("result_basic_i32"), -14);
  EXPECT_EQ(helper.Get<unsigned int>("result_basic_u8"), 16u);
  EXPECT_EQ(helper.Get<unsigned int>("result_basic_u16"), 64000u);
  EXPECT_EQ(helper.Get<unsigned int>("result_basic_u32"), 4000000000u);
  EXPECT_EQ(helper.Get<std::string>("result_later_string"), "ⓣⓔⓡⓜⓘⓝⓐⓣⓞⓡ");
  // Retrieve as a vector as there's no difference in representation in JS (and
  // gin already supports vector), and verify the length matches the expected
  // length of the fidl array.
  auto result_arrrr = helper.Get<std::vector<int32_t>>("result_arrrr");
  ASSERT_EQ(result_arrrr.size(), fidljstest::ARRRR_SIZE);
  for (uint64_t i = 0; i < fidljstest::ARRRR_SIZE; ++i) {
    EXPECT_EQ(result_arrrr[i], static_cast<int32_t>(i * 5) - 10);
  }
  EXPECT_TRUE(helper.IsNull("result_vs0"));
  EXPECT_FALSE(helper.IsNull("result_vs1"));
  auto result_vs1 = helper.Get<std::vector<std::string>>("result_vs1");
  ASSERT_EQ(result_vs1.size(), 2u);
  EXPECT_EQ(result_vs1[0], "passed_str0");
  EXPECT_EQ(result_vs1[1], "passed_str1");

  // This is a vector of enum class fidljstest::Blorp, but gin can't retrieve
  // those, so just get it as int, and cast to check values.
  auto result_vblorp = helper.Get<std::vector<int>>("result_vblorp");
  ASSERT_EQ(result_vblorp.size(), 4u);
  EXPECT_EQ(result_vblorp[0], static_cast<int>(fidljstest::Blorp::GAMMA));
  EXPECT_EQ(result_vblorp[1], static_cast<int>(fidljstest::Blorp::BETA));
  EXPECT_EQ(result_vblorp[2], static_cast<int>(fidljstest::Blorp::BETA));
  EXPECT_EQ(result_vblorp[3], static_cast<int>(fidljstest::Blorp::ALPHA));
}

TEST_F(FidlGenJsTest, DISABLED_HandlePassing) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  zx::job default_job_copy;
  ASSERT_EQ(zx::job::default_job()->duplicate(ZX_RIGHT_SAME_RIGHTS,
                                              &default_job_copy),
            ZX_OK);
  helper.runner()
      .global()
      ->Set(isolate->GetCurrentContext(),
            gin::StringToSymbol(isolate, "testJobHandle"),
            gin::ConvertToV8(isolate, default_job_copy.get()))
      .Check();

  // TODO(crbug.com/883496): Handles wrapped in Transferrable once MessagePort
  // is sorted out, and then stop treating handles as unmanaged |uint32_t|s.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.PassHandles(testJobHandle).then(h => {
      this.processHandle = h;
    }).catch((e) => log('FAILED: ' + e));
  )";
  helper.runner().Run(source, "test.js");

  // Run the message loop to send the request and receive a response.
  base::RunLoop().RunUntilIdle();

  zx_handle_t process_handle_back_from_js =
      helper.Get<uint32_t>("processHandle");
  EXPECT_EQ(GetKoidForHandle(process_handle_back_from_js),
            GetKoidForHandle(*zx::process::self()));

  // Make sure we received the valid handle back correctly, and close it. Not
  // stored into a zx::process in case it isn't valid, and to check the return
  // value from closing it.
  EXPECT_EQ(zx_handle_close(process_handle_back_from_js), ZX_OK);

  // Ensure we didn't pass away our default job, or process self.
  EXPECT_NE(GetKoidForHandle(*zx::job::default_job()), ZX_KOID_INVALID);
  EXPECT_NE(GetKoidForHandle(*zx::process::self()), ZX_KOID_INVALID);
}

TEST_F(FidlGenJsTest, DISABLED_UnionSend) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    var somu = new StructOfMultipleUnions();

    var swb = new StructWithBool(/*some_bool*/ true);
    somu.initial.set_swb(swb);

    var lswa = new LargerStructWithArray([]);
    for (var i = 0; i < 32; ++i) {
      lswa.components[i] = i * 99;
    }
    somu.optional.set_lswa(lswa);

    somu.trailing.set_swu(new StructWithUint(123456));

    proxy.ReceiveUnions(somu);
  )";
  helper.runner().Run(source, "test.js");

  base::RunLoop().RunUntilIdle();

  // Expectations on the contents of the union are checked in the body of
  // TestolaImpl::ReceiveAUnion().
  EXPECT_TRUE(testola_impl.did_receive_union());
}

TEST_F(FidlGenJsTest, DISABLED_UnionReceive) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.SendUnions().then(resp => {
      this.result_initial_is_swb = resp.initial.is_swb();
      this.result_initial_is_swu = resp.initial.is_swu();
      this.result_initial_is_lswa = resp.initial.is_lswa();
      this.result_optional_is_swb = resp.optional.is_swb();
      this.result_optional_is_swu = resp.optional.is_swu();
      this.result_optional_is_lswa = resp.optional.is_lswa();
      this.result_trailing_is_swb = resp.trailing.is_swb();
      this.result_trailing_is_swu = resp.trailing.is_swu();
      this.result_trailing_is_lswa = resp.trailing.is_lswa();

      this.result_initial_some_bool = resp.initial.swb.some_bool;
      this.result_optional_num = resp.optional.swu.num;
    }).catch((e) => log('FAILED: ' + e));
  )";
  helper.runner().Run(source, "test.js");

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper.Get<bool>("result_initial_is_swb"));
  EXPECT_FALSE(helper.Get<bool>("result_initial_is_swu"));
  EXPECT_FALSE(helper.Get<bool>("result_initial_is_lswa"));

  EXPECT_FALSE(helper.Get<bool>("result_optional_is_swb"));
  EXPECT_TRUE(helper.Get<bool>("result_optional_is_swu"));
  EXPECT_FALSE(helper.Get<bool>("result_optional_is_lswa"));

  EXPECT_FALSE(helper.Get<bool>("result_trailing_is_swb"));
  EXPECT_FALSE(helper.Get<bool>("result_trailing_is_swu"));
  EXPECT_TRUE(helper.Get<bool>("result_trailing_is_lswa"));

  EXPECT_TRUE(helper.Get<bool>("result_initial_some_bool"));
  EXPECT_EQ(helper.Get<uint32_t>("result_optional_num"), 987654u);
}

TEST_F(FidlGenJsTest, DISABLED_VariousDefaults) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  std::string source = R"(
    var temp = new VariousDefaults();
    this.result_blorp = temp.blorp_defaulting_to_beta;
    this.result_timestamp = temp.int64_defaulting_to_no_timestamp;
    this.result_another_copy = ANOTHER_COPY;
    this.result_int64_const = temp.int64_defaulting_to_const;
    this.result_string_in_struct = temp.string_with_default;
    this.result_string_const = SOME_STRING;
  )";
  helper.runner().Run(source, "test.js");

  EXPECT_EQ(helper.Get<int>("result_blorp"),
            static_cast<int>(fidljstest::Blorp::BETA));
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  EXPECT_EQ(helper.FromV8BigInt<int64_t>(
                helper.runner()
                    .global()
                    ->Get(context, gin::StringToV8(isolate, "result_timestamp"))
                    .ToLocalChecked()),
            fidljstest::NO_TIMESTAMP);
  EXPECT_EQ(
      helper.FromV8BigInt<int64_t>(
          helper.runner()
              .global()
              ->Get(context, gin::StringToV8(isolate, "result_another_copy"))
              .ToLocalChecked()),
      fidljstest::ANOTHER_COPY);
  EXPECT_EQ(
      helper.FromV8BigInt<int64_t>(
          helper.runner()
              .global()
              ->Get(context, gin::StringToV8(isolate, "result_int64_const"))
              .ToLocalChecked()),
      0x7fffffffffffff11LL);
  EXPECT_EQ(helper.Get<std::string>("result_string_const"),
            "a 你好 thing\" containing ' quotes");
  EXPECT_EQ(helper.Get<std::string>("result_string_in_struct"), "stuff");
}

TEST_F(FidlGenJsTest, DISABLED_VectorOfStrings) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    var v1 = ['str0', 'str1', 'str2'];
    var v2 = ['str3', null, null, null, 'str4'];
    var v3 = ['0123456789'];  // This is the maximum allowed length.
    proxy.SendVectorsOfString(v1, v2, v3);
  )";
  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(testola_impl.did_get_vectors_of_string());
}

TEST_F(FidlGenJsTest, DISABLED_VectorOfStringsTooLongString) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    var too_long = ['this string is longer than allowed'];
    proxy.SendVectorsOfString([], [], too_long);
    this.tried_to_send = true;
  )";
  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper.Get<bool>("tried_to_send"));
  EXPECT_FALSE(testola_impl.did_get_vectors_of_string());
}

TEST_F(FidlGenJsTest, DISABLED_VectorOfStruct) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    var data = [
      new StructWithUint(456),
      new StructWithUint(789),
      new StructWithUint(123),
      new StructWithUint(0xfffff),
    ];
    proxy.VectorOfStruct(data).then(resp => {
      this.result_length = resp.length;
      this.result_0 = resp[0].num;
      this.result_1 = resp[1].num;
    }).catch((e) => log('FAILED: ' + e));
  )";
  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(helper.Get<uint32_t>("result_length"), 2u);
  EXPECT_EQ(helper.Get<int>("result_0"), 369);
  EXPECT_EQ(helper.Get<int>("result_1"), 258);
}

TEST_F(FidlGenJsTest, DISABLED_VectorsOfPrimitives) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    var v_bool = [true];
    var v_uint8 = [2, 3];
    var v_uint16 = [4, 5, 6];
    var v_uint32 = [7, 8, 9, 10];
    var v_uint64 = [11, 12, 13, 14, 0xffffffffffffff00n];
    var v_int8 = [-16, -17, -18, -19, -20, -21];
    var v_int16 = [-22, -23, -24, -25, -26, -27, -28];
    var v_int32 = [-29, -30, -31, -32, -33, -34, -35, -36];
    var v_int64 = [-37, -38, -39, -40, -41, -42, -43, -44,
                   -0x7fffffffffffffffn];
    var v_float32 = [46, 47, 48, 49, 50, 51, 52, 53, 54, 55];
    var v_float64 = [56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66];

    var data = new VectorsOfPrimitives(
        v_bool,
        v_uint8,
        v_uint16,
        v_uint32,
        v_uint64,
        v_int8,
        v_int16,
        v_int32,
        v_int64,
        v_float32,
        v_float64);

    proxy.PassVectorOfPrimitives(data).then(resp => {
      this.result_v_bool = resp.v_bool;
      this.result_v_uint8 = resp.v_uint8;
      this.result_v_uint16 = resp.v_uint16;
      this.result_v_uint32 = resp.v_uint32;
      this.result_v_uint64 = resp.v_uint64;
      this.result_v_int8 = resp.v_int8;
      this.result_v_int16 = resp.v_int16;
      this.result_v_int32 = resp.v_int32;
      this.result_v_int64 = resp.v_int64;
      this.result_v_float32 = resp.v_float32;
      this.result_v_float64 = resp.v_float64;
    }).catch((e) => log('FAILED: ' + e));
  )";

  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  auto result_v_bool = helper.Get<std::vector<bool>>("result_v_bool");
  auto result_v_uint8 = helper.Get<std::vector<unsigned int>>("result_v_uint8");
  auto result_v_uint16 =
      helper.Get<std::vector<unsigned int>>("result_v_uint16");
  auto result_v_uint32 = helper.Get<std::vector<uint32_t>>("result_v_uint32");
  auto result_v_uint64 = helper.GetBigIntVector<uint64_t>("result_v_uint64");
  auto result_v_int8 = helper.Get<std::vector<int>>("result_v_int8");
  auto result_v_int16 = helper.Get<std::vector<int>>("result_v_int16");
  auto result_v_int32 = helper.Get<std::vector<int32_t>>("result_v_int32");
  auto result_v_int64 = helper.GetBigIntVector<int64_t>("result_v_int64");
  auto result_v_float32 = helper.Get<std::vector<float>>("result_v_float32");
  auto result_v_float64 = helper.Get<std::vector<double>>("result_v_float64");

  ASSERT_EQ(result_v_bool.size(), 1u);
  ASSERT_EQ(result_v_uint8.size(), 2u);
  ASSERT_EQ(result_v_uint16.size(), 3u);
  ASSERT_EQ(result_v_uint32.size(), 4u);
  ASSERT_EQ(result_v_uint64.size(), 5u);
  ASSERT_EQ(result_v_int8.size(), 6u);
  ASSERT_EQ(result_v_int16.size(), 7u);
  ASSERT_EQ(result_v_int32.size(), 8u);
  ASSERT_EQ(result_v_int64.size(), 9u);
  ASSERT_EQ(result_v_float32.size(), 10u);
  ASSERT_EQ(result_v_float64.size(), 11u);

  // Check that all the responses have had 10 added to them (except bool).

  EXPECT_EQ(result_v_bool[0], true);

  EXPECT_EQ(result_v_uint8[0], 12u);
  EXPECT_EQ(result_v_uint8[1], 13u);

  EXPECT_EQ(result_v_uint16[0], 14u);
  EXPECT_EQ(result_v_uint16[1], 15u);
  EXPECT_EQ(result_v_uint16[2], 16u);

  EXPECT_EQ(result_v_uint32[0], 17u);
  EXPECT_EQ(result_v_uint32[1], 18u);
  EXPECT_EQ(result_v_uint32[2], 19u);
  EXPECT_EQ(result_v_uint32[3], 20u);

  EXPECT_EQ(result_v_uint64[0], 21u);
  EXPECT_EQ(result_v_uint64[1], 22u);
  EXPECT_EQ(result_v_uint64[2], 23u);
  EXPECT_EQ(result_v_uint64[3], 24u);
  EXPECT_EQ(result_v_uint64[4], 0xffffffffffffff0aULL);

  EXPECT_EQ(result_v_int8[0], -6);
  EXPECT_EQ(result_v_int8[1], -7);
  EXPECT_EQ(result_v_int8[2], -8);
  EXPECT_EQ(result_v_int8[3], -9);
  EXPECT_EQ(result_v_int8[4], -10);
  EXPECT_EQ(result_v_int8[5], -11);

  EXPECT_EQ(result_v_int16[0], -12);
  EXPECT_EQ(result_v_int16[1], -13);
  EXPECT_EQ(result_v_int16[2], -14);
  EXPECT_EQ(result_v_int16[3], -15);
  EXPECT_EQ(result_v_int16[4], -16);
  EXPECT_EQ(result_v_int16[5], -17);
  EXPECT_EQ(result_v_int16[6], -18);

  EXPECT_EQ(result_v_int32[0], -19);
  EXPECT_EQ(result_v_int32[1], -20);
  EXPECT_EQ(result_v_int32[2], -21);
  EXPECT_EQ(result_v_int32[3], -22);
  EXPECT_EQ(result_v_int32[4], -23);
  EXPECT_EQ(result_v_int32[5], -24);
  EXPECT_EQ(result_v_int32[6], -25);
  EXPECT_EQ(result_v_int32[7], -26);

  EXPECT_EQ(result_v_int64[0], -27);
  EXPECT_EQ(result_v_int64[1], -28);
  EXPECT_EQ(result_v_int64[2], -29);
  EXPECT_EQ(result_v_int64[3], -30);
  EXPECT_EQ(result_v_int64[4], -31);
  EXPECT_EQ(result_v_int64[5], -32);
  EXPECT_EQ(result_v_int64[6], -33);
  EXPECT_EQ(result_v_int64[7], -34);
  EXPECT_EQ(result_v_int64[8], -0x7ffffffffffffff5LL);

  EXPECT_EQ(result_v_float32[0], 56.f);
  EXPECT_EQ(result_v_float32[1], 57.f);
  EXPECT_EQ(result_v_float32[2], 58.f);
  EXPECT_EQ(result_v_float32[3], 59.f);
  EXPECT_EQ(result_v_float32[4], 60.f);
  EXPECT_EQ(result_v_float32[5], 61.f);
  EXPECT_EQ(result_v_float32[6], 62.f);
  EXPECT_EQ(result_v_float32[7], 63.f);
  EXPECT_EQ(result_v_float32[8], 64.f);
  EXPECT_EQ(result_v_float32[9], 65.f);

  EXPECT_EQ(result_v_float64[0], 66.f);
  EXPECT_EQ(result_v_float64[1], 67.f);
  EXPECT_EQ(result_v_float64[2], 68.f);
  EXPECT_EQ(result_v_float64[3], 69.f);
  EXPECT_EQ(result_v_float64[4], 70.f);
  EXPECT_EQ(result_v_float64[5], 71.f);
  EXPECT_EQ(result_v_float64[6], 72.f);
  EXPECT_EQ(result_v_float64[7], 73.f);
  EXPECT_EQ(result_v_float64[8], 74.f);
  EXPECT_EQ(result_v_float64[9], 75.f);
  EXPECT_EQ(result_v_float64[10], 76.f);
}

TEST_F(FidlGenJsTest, DISABLED_VectorOfHandle) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  zx::vmo test_vmo0, test_vmo1;
  ASSERT_EQ(zx::vmo::create(4096, 0, &test_vmo0), ZX_OK);
  ASSERT_EQ(zx::vmo::create(16384, 0, &test_vmo1), ZX_OK);

  // Save to compare on return.
  zx_koid_t koid_of_vmo0 = GetKoidForHandle(test_vmo0);
  zx_koid_t koid_of_vmo1 = GetKoidForHandle(test_vmo1);

  helper.runner()
      .global()
      ->Set(isolate->GetCurrentContext(), gin::StringToSymbol(isolate, "vmo0"),
            gin::ConvertToV8(isolate, test_vmo0.release()))
      .Check();
  helper.runner()
      .global()
      ->Set(isolate->GetCurrentContext(), gin::StringToSymbol(isolate, "vmo1"),
            gin::ConvertToV8(isolate, test_vmo1.release()))
      .Check();

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    proxy.PassVectorOfVMO(new VectorOfHandleToVMO([vmo0, vmo1])).then(
    resp => {
      this.result_vmo0 = resp.vmos[0];
      this.result_vmo1 = resp.vmos[1];
    }).catch((e) => log('FAILED: ' + e));
  )";
  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  zx_handle_t result_vmo0 = helper.Get<zx_handle_t>("result_vmo0");
  zx_handle_t result_vmo1 = helper.Get<zx_handle_t>("result_vmo1");

  EXPECT_EQ(GetKoidForHandle(result_vmo0), koid_of_vmo0);
  EXPECT_EQ(GetKoidForHandle(result_vmo1), koid_of_vmo1);

  uint64_t size;
  ASSERT_EQ(zx_vmo_get_size(result_vmo0, &size), ZX_OK);
  EXPECT_EQ(size, 4096u);
  ASSERT_EQ(zx_vmo_get_size(result_vmo1, &size), ZX_OK);
  EXPECT_EQ(size, 16384u);

  EXPECT_EQ(zx_handle_close(result_vmo0), ZX_OK);
  EXPECT_EQ(zx_handle_close(result_vmo1), ZX_OK);
}

TEST_F(FidlGenJsTest, DISABLED_RequestInterface) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    var another_proxy = new AnotherInterfaceProxy();

    proxy.GetAnother(another_proxy.$request());
    this.is_bound = another_proxy.$is_bound();
    another_proxy.TimesTwo(456).then(resp => {
      this.result = resp;

      // TODO(crbug.com/883496): Handle created by $request() must be manually
      // closed for now to avoid leaking it.
      another_proxy.$close();
    }).catch((e) => log('FAILED: ' + e));

    // Use the original interface to make sure we didn't break its connection.
    proxy.PrintInt(789);
  )";
  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(helper.Get<int>("result"), 456 * 2);
  EXPECT_EQ(testola_impl.received_int(), 789);
}

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
