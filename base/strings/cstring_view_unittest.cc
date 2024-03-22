// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/cstring_view.h"

#include "base/containers/span.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <concepts>
#include <type_traits>

namespace base {
namespace {

static_assert(std::is_default_constructible_v<cstring_view>);
static_assert(std::is_trivially_copy_constructible_v<cstring_view>);
static_assert(std::is_trivially_copy_assignable_v<cstring_view>);
static_assert(std::is_trivially_move_constructible_v<cstring_view>);
static_assert(std::is_trivially_move_assignable_v<cstring_view>);
static_assert(std::is_trivially_destructible_v<cstring_view>);

static_assert(std::ranges::contiguous_range<cstring_view>);
static_assert(std::ranges::borrowed_range<cstring_view>);

// The view is the size of 2 pointers (technically, pointer and address).
static_assert(sizeof(cstring_view) == sizeof(uintptr_t) + sizeof(size_t));

TEST(CStringViewTest, DefaultConstructed) {
  constexpr auto c = cstring_view();
  static_assert(std::same_as<decltype(c), const cstring_view>);
  static_assert(c.size() == 0u);
  static_assert(c[c.size()] == '\0');
}

TEST(CStringViewTest, LiteralConstructed) {
  constexpr auto empty = cstring_view("");
  constexpr auto stuff = cstring_view("stuff");
  constexpr auto other = cstring_view("other");
  static_assert(std::same_as<decltype(empty), const cstring_view>);
  static_assert(std::same_as<decltype(stuff), const cstring_view>);
  static_assert(std::same_as<decltype(other), const cstring_view>);

  static_assert(empty.size() == 0u);
  static_assert(stuff.size() == 5u);
  static_assert(other.size() == 5u);

  static_assert(empty[empty.size()] == '\0');
  static_assert(stuff[stuff.size()] == '\0');
  static_assert(other[other.size()] == '\0');
}

TEST(CStringViewTest, PointerSizeConstructed) {
  constexpr const char* c_empty = "";
  constexpr auto empty = UNSAFE_BUFFERS(cstring_view(c_empty, 0u));
  static_assert(std::same_as<const cstring_view, decltype(empty)>);
  EXPECT_EQ(empty.data(), c_empty);
  EXPECT_EQ(empty.size(), 0u);

  constexpr const char* c_stuff = "stuff";
  constexpr auto stuff = UNSAFE_BUFFERS(cstring_view(c_stuff, 5u));
  static_assert(std::same_as<const cstring_view, decltype(stuff)>);
  EXPECT_EQ(stuff.data(), c_stuff);
  EXPECT_EQ(stuff.size(), 5u);
}

TEST(CStringViewTest, Equality) {
  constexpr auto stuff = cstring_view("stuff");

  static_assert(stuff != cstring_view());
  static_assert(stuff == cstring_view("stuff"));
  static_assert(stuff != cstring_view("other"));

  // Implicit conversion to cstring_view from literal in comparison.
  static_assert(stuff == "stuff");
}

TEST(CStringViewTest, Ordering) {
  constexpr auto stuff = cstring_view("stuff");

  static_assert(stuff <=> stuff == std::weak_ordering::equivalent);
  static_assert(stuff <=> cstring_view() == std::weak_ordering::greater);
  static_assert(stuff <=> cstring_view("stuff") ==
                std::weak_ordering::equivalent);
  static_assert(stuff <=> cstring_view("zz") == std::weak_ordering::less);

  // Implicit conversion to cstring_view from literal in ordering compare.
  static_assert(stuff <=> "stuff" == std::weak_ordering::equivalent);
}

TEST(CStringViewTest, Iterate) {
  constexpr auto def = cstring_view();
  static_assert(def.begin() == def.end());
  static_assert(def.cbegin() == def.cend());

  constexpr auto stuff = cstring_view("stuff");
  static_assert(stuff.begin() != stuff.end());
  static_assert(stuff.cbegin() != stuff.cend());
  static_assert(std::same_as<const char&, decltype(*stuff.begin())>);

  {
    size_t i = 0u;
    for (auto& c : stuff) {
      static_assert(std::same_as<const char&, decltype(c)>);
      EXPECT_EQ(&c, &stuff[i]);
      ++i;
    }
    EXPECT_EQ(i, 5u);
  }
}

TEST(CStringViewDeathTest, IterateBoundsChecked) {
  constexpr auto stuff = cstring_view("stuff");

  // The NUL terminator is out of bounds for iterating (checked by indexing into
  // the iterator) since it's not included in the range that the iterator walks
  // (but is in bounds for indexing on the view).
  BASE_EXPECT_DEATH((void)*stuff.end(), "");
  BASE_EXPECT_DEATH((void)stuff.begin()[5], "");
  BASE_EXPECT_DEATH((void)(stuff.begin() + 6), "");
  BASE_EXPECT_DEATH((void)(stuff.begin() - 1), "");
}

TEST(CStringViewTest, Size) {
  constexpr auto empty = cstring_view();
  static_assert(empty.size() == 0u);
  static_assert(empty.size_bytes() == 0u);
  constexpr auto stuff = cstring_view("stuff");
  static_assert(stuff.size() == 5u);
  static_assert(stuff.size_bytes() == 5u);

  constexpr auto empty16 = u16cstring_view();
  static_assert(empty16.size() == 0u);
  static_assert(empty16.size_bytes() == 0u);
  constexpr auto stuff16 = u16cstring_view(u"stuff");
  static_assert(stuff16.size() == 5u);
  static_assert(stuff16.size_bytes() == 10u);

  constexpr auto empty32 = u32cstring_view();
  static_assert(empty32.size() == 0u);
  static_assert(empty32.size_bytes() == 0u);
  constexpr auto stuff32 = u32cstring_view(U"stuff");
  static_assert(stuff32.size() == 5u);
  static_assert(stuff32.size_bytes() == 20u);

#if BUILDFLAG(IS_WIN)
  constexpr auto emptyw = wcstring_view();
  static_assert(emptyw.size() == 0u);
  static_assert(emptyw.size_bytes() == 0u);
  constexpr auto stuffw = wcstring_view(L"stuff");
  static_assert(stuffw.size() == 5u);
  static_assert(stuffw.size_bytes() == 10u);
#endif
}

TEST(CStringViewTest, ToSpan) {
  constexpr auto empty = cstring_view();
  {
    auto s = base::span(empty);
    static_assert(std::same_as<base::span<const char>, decltype(s)>);
    EXPECT_EQ(s.data(), empty.data());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.size_bytes(), 0u);
  }
  constexpr auto stuff = cstring_view("stuff");
  {
    auto s = base::span(stuff);
    static_assert(std::same_as<base::span<const char>, decltype(s)>);
    EXPECT_EQ(s.data(), stuff.data());
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.size_bytes(), 5u);
  }
  constexpr auto stuff16 = u16cstring_view(u"stuff");
  {
    auto s = base::span(stuff16);
    static_assert(std::same_as<base::span<const char16_t>, decltype(s)>);
    EXPECT_EQ(s.data(), stuff16.data());
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.size_bytes(), 10u);
  }
  constexpr auto stuff32 = u32cstring_view(U"stuff");
  {
    auto s = base::span(stuff32);
    static_assert(std::same_as<base::span<const char32_t>, decltype(s)>);
    EXPECT_EQ(s.data(), stuff32.data());
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.size_bytes(), 20u);
  }
}

TEST(CStringViewTest, Cstr) {
  constexpr auto empty = cstring_view();
  constexpr auto stuff = cstring_view("stuff");

  static_assert(*stuff.c_str() == 's');

  EXPECT_STREQ(empty.c_str(), "");
  EXPECT_STREQ(stuff.c_str(), "stuff");
}

TEST(CStringViewTest, Example_CtorLiteral) {
  const char kLiteral[] = "hello world";
  auto s = base::cstring_view(kLiteral);
  CHECK(s == "hello world");
  auto s2 = base::cstring_view("this works too");
  CHECK(s2 == "this works too");
}

}  // namespace
}  // namespace base
