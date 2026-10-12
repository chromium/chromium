#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import check_member_order


class CheckMemberOrderTest(unittest.TestCase):
  def test_compliant_class(self):
    content = """
class MyClass {
 public:
  MyClass();
  ~MyClass();

  void DoSomething();
  int GetCount() const { return count_; }

 protected:
  void InternalWork();

 private:
  void HelperMethod();

  int count_ = 0;
  std::string name_;
  base::WeakPtrFactory<MyClass> weak_ptr_factory_{this};
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_field_before_function_same_section(self):
    content = """
class MyClass {
 public:
  void DoSomething();

 private:
  int count_ = 0;
  void HelperMethod();
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 1)
    v = violations[0]
    self.assertEqual(v.class_name, "MyClass")
    self.assertEqual(v.field_name, "count_")
    self.assertEqual(v.func_name, "HelperMethod")

  def test_protected_field_before_private_function_allowed(self):
    content = """
class MyClass {
 protected:
  int internal_count_ = 0;

 private:
  void HelperMethod();
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_private_field_before_public_function_flagged(self):
    content = """
class MyClass {
 private:
  int count_ = 0;

 public:
  void HelperMethod();
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 1)
    v = violations[0]
    self.assertEqual(v.class_name, "MyClass")
    self.assertEqual(v.field_name, "count_")
    self.assertEqual(v.func_name, "HelperMethod")

  def test_field_before_function_in_second_private_section(self):
    content = """
class MyClass {
 public:
  void DoSomething();

 private:
  int count_ = 0;

 private:
  MyClass();
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 1)
    v = violations[0]
    self.assertEqual(v.class_name, "MyClass")
    self.assertEqual(v.field_name, "count_")
    self.assertEqual(v.func_name, "MyClass")

  def test_inlined_method_bodies_ignored(self):
    content = """
class MyClass {
 public:
  void Compute() {
    int local_var = 10;
    int state_ = 20;
    return state_;
  }

  void AnotherMethod();

 private:
  int actual_field_ = 0;
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_static_constants_allowed_before_functions(self):
    content = """
class MyClass {
 public:
  static constexpr int kMaxRetries = 3;
  static const char* kDefaultName;

  MyClass();
  void Run();

 private:
  int retries_ = 0;
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_static_const_and_constexpr_functions_recognized(self):
    content = """
class MyClass {
 private:
  int retries_ = 0;
  static const char* GetDefaultName();
  static constexpr int CalculateMax() { return 10; }
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 2)
    self.assertEqual(violations[0].func_name, "GetDefaultName")
    self.assertEqual(violations[1].func_name, "CalculateMax")

  def test_type_aliases_and_friends(self):
    content = """
class MyClass {
 public:
  using Callback = base::OnceCallback<void()>;
  typedef int MyInt;

  friend class FriendClass;
  friend void FriendFunction();

  void Run();

 private:
  Callback callback_;
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_macros_not_confused_with_functions(self):
    content = """
class MyClass : public views::View {
 public:
  METADATA_HEADER(MyClass, views::View);
  WEB_UI_CONTROLLER_TYPE_DECL();

  MyClass();
  ~MyClass() override;

 private:
  int state_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_mock_methods_recognized_as_functions(self):
    content = """
class MockFoo {
 public:
  MOCK_METHOD(void, OnEvent, (int event_id), (override));

 private:
  int expected_event_id_ = 0;
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_mock_method_after_field_is_flagged(self):
    content = """
class MockFoo {
 private:
  int expected_event_id_ = 0;

 public:
  MOCK_METHOD(void, OnEvent, (int event_id), (override));
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 1)
    self.assertEqual(violations[0].func_name, "MOCK_METHOD")

  def test_thread_annotations_on_fields(self):
    content = """
class ThreadSafeClass {
 public:
  void DoWork();

 private:
  mutable base::Lock lock_;
  int counter_ GUARDED_BY(lock_) = 0;
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_field_initializers_with_function_calls(self):
    content = """
class Controller {
 public:
  Controller();
  void Start();

 private:
  base::TimeDelta delay_ = base::Hours(23);
  std::unique_ptr<Helper> helper_ = Helper::CreateDefault();
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_nested_classes_scopes_separated(self):
    content = """
class Outer {
 public:
  class Inner {
   public:
    void InnerMethod();
   private:
    int inner_field_ = 0;
  };

  void OuterMethod();

 private:
  int outer_field_ = 0;
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_comments_and_strings_with_code_syntax(self):
    content = """
// class CommentClass { int comment_field_; void CommentFunc(); };
class RealClass {
 public:
  void Run() {
    const char* str = "class FakeClass { int fake_; void Fake(); };";
  }

 private:
  int real_field_ = 0;
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_brace_initialized_fields(self):
    content = """
class Container {
 private:
  std::vector<int> numbers_{1, 2, 3};
  base::WeakPtrFactory<Container> weak_factory_{this};
  void AfterBraceInit();
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 1)
    self.assertEqual(violations[0].field_name, "numbers_")
    self.assertEqual(violations[0].func_name, "AfterBraceInit")

  def test_preprocessor_directives(self):
    content = """
class PlatformService {
 private:
  int base_field_ = 0;
#if BUILDFLAG(IS_ANDROID)
  void AndroidHelper();
#endif
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 1)
    self.assertEqual(violations[0].field_name, "base_field_")
    self.assertEqual(violations[0].func_name, "AndroidHelper")

  def test_lowercase_and_acronym_methods(self):
    content = """
class Widget {
 private:
  int state_ = 0;
  int size() const;
  void URL();
  void UI();
  Profile* profile();
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 4)
    func_names = [v.func_name for v in violations]
    self.assertEqual(func_names, ["size", "URL", "UI", "profile"])

  def test_complex_param_and_qualifier_methods(self):
    content = """
class AsyncRunner {
 private:
  int task_count_ = 0;
  void Run() noexcept override;
  void Register(base::RepeatingCallback<void(int, std::string)> cb) const {}
  void Wait(base::TimeDelta timeout = base::Seconds(5));
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 3)
    func_names = [v.func_name for v in violations]
    self.assertEqual(func_names, ["Run", "Register", "Wait"])

  def test_struct_with_violations_flagged(self):
    content = """
struct DataStruct {
  int data_ = 0;
  void Reset();
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 1)
    self.assertEqual(violations[0].class_name, "DataStruct")
    self.assertEqual(violations[0].field_name, "data_")
    self.assertEqual(violations[0].func_name, "Reset")

  def test_exact_line_numbers_reported(self):
    content = (
      "class NumberedTest {\n"  # line 1
      " private:\n"  # line 2
      "  // Comment before field\n"  # line 3
      "  int field_ = 0;\n"  # line 4
      "\n"  # line 5
      "  // Comment before func\n"  # line 6
      "  void Method();\n"  # line 7
      "};\n"
    )
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(len(violations), 1)
    self.assertEqual(violations[0].field_line, 4)
    self.assertEqual(violations[0].func_line, 7)

  def test_guarded_by_context_on_fields(self):
    content = """
class ThreadCheckerClass {
 public:
  void DoWork();

 private:
  const base::FilePath modules_dir_;
  VersionSetCallback on_version_set_changed_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::DelayTimer debounce_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_guarded_by_context_with_brace_init(self):
    content = """
class ObservableClass {
 public:
  void Run();

 private:
  base::ScopedObservation<Source, Observer> observer_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_static_assert_allowed_after_fields(self):
    content = """
class AssertClass {
 public:
  void Run();

 private:
  int value_ = 0;
  static_assert(sizeof(int) == 4);
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_literal_call_in_branching_initializer(self):
    content = """
class BranchingInitClass {
 public:
  void Run();

 private:
  std::optional<int> count_;
  bool flag_ =
#if BUILDFLAG(IS_ANDROID)
      ComputeFlag(true);
#else
      ComputeFlag(false);
#endif
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])

  def test_func_param_ending_with_underscore_not_a_field(self):
    content = """
class ParamClass {
 public:
  explicit ParamClass(int arg_ = 0);
  void Hide(bool suppress_animation_ = false);
  void Show();

 private:
  int real_field_ = 0;
};
"""
    violations = check_member_order.find_violations_in_content(content)
    self.assertEqual(violations, [])


if __name__ == '__main__':
  unittest.main()
