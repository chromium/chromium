// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fuchsia/element_manager_impl.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

struct AnnotationKeyProxy {
  AnnotationKeyProxy(
      std::string key,
      std::string namespace_ = "namespace")  // NOLINT(runtime/explicit)
      : key(std::move(key)), namespace_(std::move(namespace_)) {}
  std::string key;
  std::string namespace_;

  fuchsia::element::AnnotationKey ToAnnotationKey() const {
    fuchsia::element::AnnotationKey result;
    result.value = key;
    result.namespace_ = namespace_;
    return result;
  }
};

struct AnnotationProxy {
  AnnotationProxy(std::string key, std::string value)
      : key(std::move(key)), value(std::move(value)) {}
  AnnotationProxy(std::string key, std::string namespace_, std::string value)
      : key(std::move(key), std::move(namespace_)), value(std::move(value)) {}

  AnnotationKeyProxy key;
  std::string value;

  fuchsia::element::Annotation ToAnnotation() const {
    fuchsia::element::Annotation result;
    result.key = key.ToAnnotationKey();
    result.value =
        fuchsia::element::AnnotationValue::WithText(std::string(value));
    return result;
  }
};

std::vector<fuchsia::element::AnnotationKey> TestAnnotationKeys(
    std::initializer_list<AnnotationKeyProxy> keys) {
  std::vector<fuchsia::element::AnnotationKey> results;
  for (const auto& key : keys) {
    results.push_back(key.ToAnnotationKey());
  }
  return results;
}

std::vector<fuchsia::element::Annotation> TestAnnotations(
    std::initializer_list<AnnotationProxy> annotations) {
  std::vector<fuchsia::element::Annotation> results;
  for (const auto& annotation : annotations) {
    results.push_back(annotation.ToAnnotation());
  }
  return results;
}

class ElementManagerImplTest : public testing::Test {
 public:
  ElementManagerImplTest()
      : element_manager_(base::ComponentContextForProcess()->outgoing().get(),
                         base::BindLambdaForTesting(
                             [&](const base::CommandLine& command_line) {
                               received_command_line_ = command_line;
                               return true;
                             })) {
    element_manager_.set_have_browser_callback_for_test(
        base::BindLambdaForTesting([&]() { return browser_count_ > 0; }));
  }

 protected:
  fuchsia::element::ManagerPtr GetElementManagerPtr() {
    return test_context_.published_services()
        ->Connect<fuchsia::element::Manager>();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::TestComponentContextForProcess test_context_;
  absl::optional<base::CommandLine> received_command_line_;
  ElementManagerImpl element_manager_;
  int browser_count_ = 0;
};

TEST_F(ElementManagerImplTest, CorrectSpec) {
  auto element_manager = GetElementManagerPtr();
  for (const char* url : {
           "fuchsia-pkg://fuchsia.com/chrome#meta/chrome.cm",
           "fuchsia-pkg://chromium.org/chrome#meta/chrome.cm",
           "fuchsia-pkg://chrome.com/chrome#meta/chrome.cm",
           "http://www.example.com",
           "https://www.example.com",
       }) {
    fuchsia::element::Spec spec;
    spec.set_component_url(url);

    base::RunLoop run_loop;
    absl::optional<fuchsia::element::Manager_ProposeElement_Result>
        received_result;
    element_manager->ProposeElement(
        std::move(spec), {},
        [&](fuchsia::element::Manager_ProposeElement_Result result) {
          received_result = std::move(result);
          run_loop.Quit();
        });
    run_loop.Run();
    ASSERT_TRUE(received_result);
    EXPECT_FALSE(received_result->is_err()) << url;
    EXPECT_TRUE(received_command_line_);
  }
}

TEST_F(ElementManagerImplTest, IncorrectSpec) {
  auto element_manager = GetElementManagerPtr();
  for (const char* url : {
           "foobar",
           "fuchsia-pkg://chromium.org/web_engine#meta/chrome.cm",
           "fuchsia-pkg://chromium.org/chrome#meta/web_engine.cm",
           "fuchsia-pkg://chromium.org/chrome",
           "fuchsia-pkg://chromium.org/#meta/chrome.cm",
           "fuchsia-pkg://chromium.org/mychrome#meta/chrome.cm",
           "chrome#meta/chrome.cm",
       }) {
    fuchsia::element::Spec spec;
    spec.set_component_url(url);

    base::RunLoop run_loop;
    absl::optional<fuchsia::element::Manager_ProposeElement_Result>
        received_result;
    element_manager->ProposeElement(
        std::move(spec), {},
        [&](fuchsia::element::Manager_ProposeElement_Result result) {
          received_result = std::move(result);
          run_loop.Quit();
        });
    run_loop.Run();
    ASSERT_TRUE(received_result);
    EXPECT_TRUE(received_result->is_err()) << url;
    EXPECT_FALSE(received_command_line_);
  }
}

TEST_F(ElementManagerImplTest, ElementControllerClosedOnInvalidSpec) {
  auto element_manager = GetElementManagerPtr();

  fuchsia::element::ControllerPtr controller;
  fuchsia::element::Spec valid_spec;
  valid_spec.set_component_url(
      "fuchsia-pkg://chromium.org/chrome#meta/chrome.cm");

  element_manager->ProposeElement(
      std::move(valid_spec), controller.NewRequest(),
      [&](auto result) { ASSERT_TRUE(result.is_response()); });
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller.is_bound());

  fuchsia::element::Spec invalid_spec;
  invalid_spec.set_component_url("foobar");

  controller = fuchsia::element::ControllerPtr();
  element_manager->ProposeElement(
      std::move(invalid_spec), controller.NewRequest(),
      [&](auto result) { ASSERT_FALSE(result.is_response()); });
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller.is_bound());
}

TEST_F(ElementManagerImplTest, Annotations) {
  EXPECT_EQ(0u, element_manager_.annotations_manager().GetAnnotations().size());

  auto element_manager = GetElementManagerPtr();

  fuchsia::element::ControllerPtr controller;

  {
    base::RunLoop run_loop;
    fuchsia::element::Spec valid_spec;
    valid_spec.set_component_url(
        "fuchsia-pkg://chromium.org/chrome#meta/chrome.cm");
    *valid_spec.mutable_annotations() = TestAnnotations(
        {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}});
    element_manager->ProposeElement(std::move(valid_spec),
                                    controller.NewRequest(), [&](auto result) {
                                      ASSERT_TRUE(result.is_response());
                                      run_loop.Quit();
                                    });
    run_loop.Run();
  }

  std::vector<fuchsia::element::Annotation> annotations =
      element_manager_.annotations_manager().GetAnnotations();
  EXPECT_EQ(3u, annotations.size());
  for (const auto* key : {"key1", "key2", "key3"}) {
    EXPECT_TRUE(base::Contains(annotations, key, [](const auto& annotation) {
      return annotation.key.value;
    }));
  }

  {
    base::RunLoop run_loop;
    controller->GetAnnotations([&](auto result) {
      ASSERT_TRUE(result.is_response());
      annotations = std::move(result.response().annotations);
      run_loop.Quit();
    });
    run_loop.Run();
  }
  EXPECT_EQ(3u, annotations.size());

  {
    base::RunLoop run_loop;
    controller->UpdateAnnotations(
        TestAnnotations({{"key1", "other_value1"},
                         {"key4", "value4"},
                         {"key3", "other_namespace", "value5"}}),
        TestAnnotationKeys(
            {{"key1", "nonexistant_namespace"}, {"key2"}, {"key3"}}),
        [&](auto result) {
          ASSERT_TRUE(result.is_response());
          run_loop.Quit();
        });
    run_loop.Run();
  }
  annotations = element_manager_.annotations_manager().GetAnnotations();
  EXPECT_EQ(3u, annotations.size());
  for (const auto* key : {"key1", "key3", "key4"}) {
    EXPECT_TRUE(base::Contains(annotations, key, [](const auto& annotation) {
      return annotation.key.value;
    }));
  }
}

TEST_F(ElementManagerImplTest, ElementControllerAndBrowserLifeCycle) {
  auto element_manager = GetElementManagerPtr();

  fuchsia::element::ControllerPtr controller;
  fuchsia::element::Spec valid_spec;
  valid_spec.set_component_url(
      "fuchsia-pkg://chromium.org/chrome#meta/chrome.cm");

  element_manager->ProposeElement(
      std::move(valid_spec), controller.NewRequest(),
      [&](auto result) { ASSERT_TRUE(result.is_response()); });
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller.is_bound());

  // Notifying the manager while there is still browser alive should not
  // disconnect the controller.
  browser_count_ = 1;
  static_cast<BrowserListObserver*>(&element_manager_)
      ->OnBrowserRemoved(nullptr);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller.is_bound());

  // When the manager is notified for the last window, the controller should be
  // unbound.
  browser_count_ = 0;
  static_cast<BrowserListObserver*>(&element_manager_)
      ->OnBrowserRemoved(nullptr);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(controller.is_bound());

  // A new connection to the manager should allow a new controller to be bounded
  valid_spec.set_component_url(
      "fuchsia-pkg://chromium.org/chrome#meta/chrome.cm");

  element_manager->ProposeElement(
      std::move(valid_spec), controller.NewRequest(),
      [&](auto result) { ASSERT_TRUE(result.is_response()); });
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(controller.is_bound());
}

}  // namespace
