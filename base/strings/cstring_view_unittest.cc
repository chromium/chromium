// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/cstring_view.h"

#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <concepts>
#include <limits>
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

TEST(CStringViewTest, StringConstructed) {
  std::string empty;
  {
    auto c = cstring_view(empty);
    EXPECT_EQ(c.size(), 0u);
  }
  std::string stuff = "stuff";
  {
    auto c = cstring_view(stuff);
    EXPECT_EQ(c.c_str(), stuff.c_str());
    EXPECT_EQ(c.size(), 5u);
  }
  std::u16string stuff16 = u"stuff";
  {
    auto c = u16cstring_view(stuff16);
    EXPECT_EQ(c.c_str(), stuff16.c_str());
    EXPECT_EQ(c.size(), 5u);
  }
  std::u32string stuff32 = U"stuff";
  {
    auto c = u32cstring_view(stuff32);
    EXPECT_EQ(c.c_str(), stuff32.c_str());
    EXPECT_EQ(c.size(), 5u);
  }
#if BUILDFLAG(IS_WIN)
  std::wstring stuffw = L"stuff";
  {
    auto c = wcstring_view(stuffw);
    EXPECT_EQ(c.c_str(), stuffw.c_str());
    EXPECT_EQ(c.size(), 5u);
  }
#endif
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

TEST(CStringViewTest, IterateReverse) {
  constexpr auto def = cstring_view();
  static_assert(def.rbegin() == def.rend());
  static_assert(def.rcbegin() == def.rcend());

  constexpr auto stuff = cstring_view("stuff");
  static_assert(stuff.rbegin() != stuff.rend());
  static_assert(stuff.rcbegin() != stuff.rcend());
  static_assert(std::same_as<const char&, decltype(*stuff.rbegin())>);

  {
    size_t i = 0u;
    for (auto it = stuff.rbegin(); it != stuff.rend(); ++it) {
      static_assert(std::same_as<const char&, decltype(*it)>);
      EXPECT_EQ(&*it, &stuff[4u - i]);
      ++i;
    }
    EXPECT_EQ(i, 5u);
  }
}

TEST(CStringViewDeathTest, IterateBoundsChecked) {
  auto use = [](auto x) { base::debug::Alias(&x); };

  constexpr auto stuff = cstring_view("stuff");

  // The NUL terminator is out of bounds for iterating (checked by indexing into
  // the iterator) since it's not included in the range that the iterator walks
  // (but is in bounds for indexing on the view).
  BASE_EXPECT_DEATH(use(*stuff.end()), "");       // Can't deref end.
  BASE_EXPECT_DEATH(use(stuff.begin()[5]), "");   // Can't index end.
  BASE_EXPECT_DEATH(use(stuff.begin() + 6), "");  // Can't move past end.
  BASE_EXPECT_DEATH(use(stuff.begin() - 1), "");  // Can't move past begin.

  BASE_EXPECT_DEATH(use(*stuff.rend()), "");
  BASE_EXPECT_DEATH(use(stuff.rbegin()[5]), "");
  BASE_EXPECT_DEATH(use(stuff.rbegin() + 6), "");
  BASE_EXPECT_DEATH(use(stuff.rbegin() - 1), "");
}

TEST(CStringViewTest, Index) {
  constexpr auto empty = cstring_view();
  static_assert(empty[0u] == '\0');

  static_assert(empty.at(0u) == '\0');

  constexpr auto stuff = cstring_view("stuff");
  static_assert(stuff[0u] == 's');
  static_assert(&stuff[0u] == stuff.data());
  static_assert(stuff[5u] == '\0');
  static_assert(&stuff[5u] == UNSAFE_BUFFERS(stuff.data() + 5u));

  static_assert(stuff.at(0u) == 's');
  static_assert(&stuff.at(0u) == stuff.data());
  static_assert(stuff.at(5u) == '\0');
  static_assert(&stuff.at(5u) == UNSAFE_BUFFERS(stuff.data() + 5u));
}

TEST(CStringViewDeathTest, IndexChecked) {
  auto use = [](auto x) { base::debug::Alias(&x); };

  constexpr auto empty = cstring_view();
  BASE_EXPECT_DEATH(use(empty[1u]), "");
  BASE_EXPECT_DEATH(use(empty[std::numeric_limits<size_t>::max()]), "");

  BASE_EXPECT_DEATH(use(empty.at(1u)), "");
  BASE_EXPECT_DEATH(use(empty.at(std::numeric_limits<size_t>::max())), "");

  constexpr auto stuff = cstring_view("stuff");
  BASE_EXPECT_DEATH(use(stuff[6u]), "");
  BASE_EXPECT_DEATH(use(stuff[std::numeric_limits<size_t>::max()]), "");

  BASE_EXPECT_DEATH(use(stuff.at(6u)), "");
  BASE_EXPECT_DEATH(use(stuff.at(std::numeric_limits<size_t>::max())), "");
}

TEST(CStringViewTest, FrontBack) {
  constexpr auto stuff = cstring_view("stuff");
  static_assert(stuff.front() == 's');
  static_assert(&stuff.front() == stuff.data());
  static_assert(stuff.back() == 'f');
  static_assert(&stuff.back() == UNSAFE_BUFFERS(stuff.data() + 4u));

  constexpr auto one = cstring_view("1");
  static_assert(one.front() == '1');
  static_assert(&one.front() == one.data());
  static_assert(one.back() == '1');
  static_assert(&one.back() == one.data());
}

TEST(CStringViewDeathTest, FrontBackChecked) {
  auto use = [](auto x) { base::debug::Alias(&x); };

  constexpr auto empty = cstring_view();
  BASE_EXPECT_DEATH(use(empty.front()), "");
  BASE_EXPECT_DEATH(use(empty.back()), "");
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

TEST(CStringViewTest, Empty) {
  constexpr auto empty = cstring_view();
  static_assert(empty.empty());
  constexpr auto one = cstring_view("1");
  static_assert(!one.empty());
  constexpr auto stuff = cstring_view("stuff");
  static_assert(!stuff.empty());

  constexpr auto empty16 = u16cstring_view();
  static_assert(empty16.empty());
  constexpr auto stuff16 = u16cstring_view(u"stuff");
  static_assert(!stuff16.empty());

  constexpr auto empty32 = u32cstring_view();
  static_assert(empty32.empty());
  constexpr auto stuff32 = u32cstring_view(U"stuff");
  static_assert(!stuff32.empty());

#if BUILDFLAG(IS_WIN)
  constexpr auto emptyw = wcstring_view();
  static_assert(emptyw.empty());
  constexpr auto stuffw = wcstring_view(L"stuff");
  static_assert(!stuffw.empty());
#endif
}

TEST(CStringViewTest, MaxSize) {
  static_assert(cstring_view().max_size() ==
                std::numeric_limits<size_t>::max());
  static_assert(u16cstring_view().max_size() ==
                std::numeric_limits<size_t>::max() / 2u);
  static_assert(u32cstring_view().max_size() ==
                std::numeric_limits<size_t>::max() / 4u);
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view().max_size() ==
                std::numeric_limits<size_t>::max() / 2u);
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

TEST(CStringViewTest, CopyConstuct) {
  static_assert(std::is_trivially_copy_constructible_v<cstring_view>);

  auto stuff = cstring_view("stuff");
  auto other = stuff;
  EXPECT_EQ(other.data(), stuff.data());
  EXPECT_EQ(other.size(), stuff.size());
}

TEST(CStringViewTest, CopyAssign) {
  static_assert(std::is_trivially_copy_assignable_v<cstring_view>);

  auto empty = cstring_view();
  auto stuff = cstring_view("stuff");
  empty = stuff;
  EXPECT_EQ(empty.data(), stuff.data());
  EXPECT_EQ(empty.size(), stuff.size());
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
