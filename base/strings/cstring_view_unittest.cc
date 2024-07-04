// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/cstring_view.h"

#include <concepts>
#include <limits>
#include <sstream>
#include <type_traits>

#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

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

static_assert(cstring_view::npos == std::string_view::npos);
static_assert(u16cstring_view::npos == std::u16string_view::npos);
static_assert(u16cstring_view::npos == std::u32string_view::npos);
#if BUILDFLAG(IS_WIN)
static_assert(wcstring_view::npos == std::wstring_view::npos);
#endif

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

  // Implicit construction.
  {
    cstring_view s = "stuff";
    EXPECT_EQ(s, cstring_view("stuff"));
  }
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

  // Implicit construction.
  {
    auto s = std::string("stuff");
    cstring_view v = s;
    EXPECT_EQ(v, cstring_view("stuff"));
  }
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

TEST(CStringViewTest, RemovePrefix) {
  auto empty = cstring_view();
  auto mod_empty = empty;
  mod_empty.remove_prefix(0u);
  EXPECT_EQ(mod_empty.data(), &empty[0u]);
  EXPECT_EQ(mod_empty.size(), 0u);

  auto stuff = cstring_view("stuff");
  auto mod_stuff = stuff;
  mod_stuff.remove_prefix(0u);
  EXPECT_EQ(mod_stuff.data(), &stuff[0u]);
  EXPECT_EQ(mod_stuff.size(), 5u);
  mod_stuff.remove_prefix(2u);
  EXPECT_EQ(mod_stuff.data(), &stuff[2u]);
  EXPECT_EQ(mod_stuff.size(), 3u);
  mod_stuff.remove_prefix(1u);
  EXPECT_EQ(mod_stuff.data(), &stuff[3u]);
  EXPECT_EQ(mod_stuff.size(), 2u);
  mod_stuff.remove_prefix(2u);
  EXPECT_EQ(mod_stuff.data(), &stuff[5u]);
  EXPECT_EQ(mod_stuff.size(), 0u);

  static_assert([] {
    auto stuff = cstring_view("stuff");
    stuff.remove_prefix(2u);
    return stuff;
  }() == "uff");

  auto stuff16 = u16cstring_view(u"stuff");
  auto mod_stuff16 = stuff16;
  mod_stuff16.remove_prefix(2u);
  EXPECT_EQ(mod_stuff16.data(), &stuff16[2u]);
  EXPECT_EQ(mod_stuff16.size(), 3u);

  auto stuff32 = u32cstring_view(U"stuff");
  auto mod_stuff32 = stuff32;
  mod_stuff32.remove_prefix(2u);
  EXPECT_EQ(mod_stuff32.data(), &stuff32[2u]);
  EXPECT_EQ(mod_stuff32.size(), 3u);

#if BUILDFLAG(IS_WIN)
  auto stuffw = wcstring_view(L"stuff");
  auto mod_stuffw = stuffw;
  mod_stuffw.remove_prefix(2u);
  EXPECT_EQ(mod_stuffw.data(), &stuffw[2u]);
  EXPECT_EQ(mod_stuffw.size(), 3u);
#endif
}

TEST(CStringViewDeathTest, RemovePrefixChecked) {
  auto empty = cstring_view();
  BASE_EXPECT_DEATH(empty.remove_prefix(1u), "");

  auto stuff = cstring_view("stuff");
  BASE_EXPECT_DEATH(stuff.remove_prefix(6u), "");
  stuff.remove_prefix(4u);
  BASE_EXPECT_DEATH(stuff.remove_prefix(2u), "");
}

TEST(CStringViewTest, Swap) {
  auto empty = cstring_view();
  auto stuff = cstring_view("stuff");
  empty.swap(stuff);
  EXPECT_EQ(stuff, cstring_view(""));
  EXPECT_EQ(empty, cstring_view("stuff"));

  static_assert([] {
    auto abc = cstring_view("abc");
    auto ef = cstring_view("ef");
    abc.swap(ef);
    return ef;
  }() == "abc");

  auto one16 = u16cstring_view(u"one");
  auto two16 = u16cstring_view(u"twotwo");
  one16.swap(two16);
  EXPECT_EQ(one16, u16cstring_view(u"twotwo"));
  EXPECT_EQ(two16, u16cstring_view(u"one"));
}

TEST(CStringViewTest, Substr) {
  auto substr = cstring_view("hello").substr(1u);
  static_assert(std::same_as<std::string_view, decltype(substr)>);

  static_assert(cstring_view("").substr(0u) == "");
  static_assert(cstring_view("").substr(0u, 0u) == "");
  static_assert(cstring_view("stuff").substr(0u) == "stuff");
  static_assert(cstring_view("stuff").substr(0u, 2u) == "st");
  static_assert(cstring_view("stuff").substr(2u) == "uff");
  static_assert(cstring_view("stuff").substr(2u, 3u) == "uff");
  static_assert(cstring_view("stuff").substr(2u, 1u) == "u");
  static_assert(cstring_view("stuff").substr(2u, 0u) == "");

  // `count` going off the end is clamped. Same as for string_view with
  // hardening.
  static_assert(cstring_view("stuff").substr(2u, 4u) == "uff");
  static_assert(std::string_view("stuff").substr(2u, 4u) == "uff");
}

TEST(CStringViewDeathTest, SubstrBoundsChecked) {
  auto use = [](auto x) { base::debug::Alias(&x); };

  auto stuff = cstring_view("stuff");

  // `pos` going off the end is CHECKed. Same as for string_view with hardening.
  BASE_EXPECT_DEATH(use(stuff.substr(6u, 0u)), "");
  BASE_EXPECT_DEATH(use(std::string_view("stuff").substr(6u, 0u)), "");
  BASE_EXPECT_DEATH(use(stuff.substr(6u, 1u)), "");
  BASE_EXPECT_DEATH(use(std::string_view("stuff").substr(6u, 1u)), "");
}

TEST(CStringViewTest, StartsWith) {
  // Comparison with `const char*`.
  static_assert(!cstring_view("").starts_with("hello"));
  static_assert(cstring_view("").starts_with(""));
  static_assert(cstring_view("hello").starts_with("hello"));
  static_assert(cstring_view("hello").starts_with(""));
  static_assert(cstring_view("hello").starts_with("he"));
  static_assert(!cstring_view("hello").starts_with("ello"));
  constexpr const char* query = "ello";
  static_assert(!cstring_view("hello").starts_with(query));

  static_assert(u16cstring_view(u"hello").starts_with(u"he"));
  static_assert(u32cstring_view(U"hello").starts_with(U"he"));
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").starts_with(L"he"));
#endif

  // Comparison with `string/string_view/cstring_view`.
  static_assert(cstring_view("hello").starts_with(std::string("he")));
  static_assert(!cstring_view("hello").starts_with(std::string("el")));
  static_assert(cstring_view("hello").starts_with(std::string_view("he")));
  static_assert(!cstring_view("hello").starts_with(std::string_view("el")));
  static_assert(cstring_view("hello").starts_with(cstring_view("he")));
  static_assert(!cstring_view("hello").starts_with(cstring_view("el")));

  static_assert(!cstring_view("hello").starts_with(std::string("hellos")));
  static_assert(!cstring_view("hello").starts_with(std::string_view("hellos")));
  static_assert(!cstring_view("hello").starts_with(cstring_view("hellos")));

  static_assert(u16cstring_view(u"hello").starts_with(std::u16string(u"he")));
  static_assert(u32cstring_view(U"hello").starts_with(std::u32string(U"he")));
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").starts_with(std::wstring(L"he")));
#endif

  // Comparison with a character.
  static_assert(!cstring_view("").starts_with('h'));
  static_assert(cstring_view("hello").starts_with('h'));
  static_assert(!cstring_view("hello").starts_with('e'));

  static_assert(u16cstring_view(u"hello").starts_with(u'h'));
  static_assert(u32cstring_view(U"hello").starts_with(U'h'));
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").starts_with(L'h'));
#endif
}

TEST(CStringViewTest, EndsWith) {
  // Comparison with `const char*`.
  static_assert(!cstring_view("").ends_with("hello"));
  static_assert(cstring_view("").ends_with(""));
  static_assert(cstring_view("hello").ends_with("hello"));
  static_assert(cstring_view("hello").ends_with(""));
  static_assert(cstring_view("hello").ends_with("lo"));
  static_assert(!cstring_view("hello").ends_with("hel"));
  constexpr const char* query = "hel";
  static_assert(!cstring_view("hello").ends_with(query));

  static_assert(u16cstring_view(u"hello").ends_with(u"lo"));
  static_assert(u32cstring_view(U"hello").ends_with(U"lo"));
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").ends_with(L"lo"));
#endif

  // Comparison with `string/string_view/cstring_view`.
  static_assert(cstring_view("hello").ends_with(std::string("lo")));
  static_assert(!cstring_view("hello").ends_with(std::string("ell")));
  static_assert(cstring_view("hello").ends_with(std::string_view("lo")));
  static_assert(!cstring_view("hello").ends_with(std::string_view("ell")));
  static_assert(cstring_view("hello").ends_with(cstring_view("lo")));
  static_assert(!cstring_view("hello").ends_with(cstring_view("ell")));

  static_assert(!cstring_view("hello").ends_with(std::string("shello")));
  static_assert(!cstring_view("hello").ends_with(std::string_view("shello")));
  static_assert(!cstring_view("hello").ends_with(cstring_view("shello")));

  static_assert(u16cstring_view(u"hello").ends_with(std::u16string(u"lo")));
  static_assert(u32cstring_view(U"hello").ends_with(std::u32string(U"lo")));
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").ends_with(std::wstring(L"lo")));
#endif

  // Comparison with a character.
  static_assert(!cstring_view("").ends_with('h'));
  static_assert(cstring_view("hello").ends_with('o'));
  static_assert(!cstring_view("hello").ends_with('l'));

  static_assert(u16cstring_view(u"hello").ends_with(u'o'));
  static_assert(u32cstring_view(U"hello").ends_with(U'o'));
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").ends_with(L'o'));
#endif
}

TEST(CStringViewTest, Find) {
  // OOB `pos` will return npos. The NUL is never searched.
  static_assert(cstring_view("hello").find('h', 1000u) == cstring_view::npos);
  static_assert(cstring_view("hello").find('\0', 5u) == cstring_view::npos);

  // Searching for a Char.
  static_assert(cstring_view("hello").find('e') == 1u);
  static_assert(cstring_view("hello").find('z') == cstring_view::npos);
  static_assert(cstring_view("hello").find('l') == 2u);
  static_assert(cstring_view("hello").find('l', 3u) == 3u);

  static_assert(u16cstring_view(u"hello").find(u'e') == 1u);
  static_assert(u16cstring_view(u"hello").find(u'z') == cstring_view::npos);
  static_assert(u16cstring_view(u"hello").find(u'l') == 2u);
  static_assert(u16cstring_view(u"hello").find(u'l', 3u) == 3u);

  static_assert(u32cstring_view(U"hello").find(U'e') == 1u);
  static_assert(u32cstring_view(U"hello").find(U'z') == cstring_view::npos);
  static_assert(u32cstring_view(U"hello").find(U'l') == 2u);
  static_assert(u32cstring_view(U"hello").find(U'l', 3u) == 3u);

#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").find(L'e') == 1u);
  static_assert(wcstring_view(L"hello").find(L'z') == cstring_view::npos);
  static_assert(wcstring_view(L"hello").find(L'l') == 2u);
  static_assert(wcstring_view(L"hello").find(L'l', 3u) == 3u);
#endif

  // Searching for a string.
  static_assert(cstring_view("hello hello").find("lo") == 3u);
  static_assert(cstring_view("hello hello").find("lol") == cstring_view::npos);
  static_assert(u16cstring_view(u"hello hello").find(u"lo") == 3u);
  static_assert(u16cstring_view(u"hello hello").find(u"lol") ==
                cstring_view::npos);
  static_assert(u32cstring_view(U"hello hello").find(U"lo") == 3u);
  static_assert(u32cstring_view(U"hello hello").find(U"lol") ==
                cstring_view::npos);
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello hello").find(L"lo") == 3u);
  static_assert(wcstring_view(L"hello hello").find(L"lol") ==
                cstring_view::npos);
#endif
}

TEST(CStringViewTest, Rfind) {
  // OOB `pos` will clamp to the end of the view. The NUL is never searched.
  static_assert(cstring_view("hello").rfind('h', 0u) == 0u);
  static_assert(cstring_view("hello").rfind('h', 1000u) == 0u);
  static_assert(cstring_view("hello").rfind('\0', 5u) == cstring_view::npos);

  // Searching for a Char.
  static_assert(cstring_view("hello").rfind('e') == 1u);
  static_assert(cstring_view("hello").rfind('z') == cstring_view::npos);
  static_assert(cstring_view("hello").rfind('l') == 3u);
  static_assert(cstring_view("hello").rfind('l', 2u) == 2u);

  static_assert(u16cstring_view(u"hello").rfind(u'e') == 1u);
  static_assert(u16cstring_view(u"hello").rfind(u'z') == cstring_view::npos);
  static_assert(u16cstring_view(u"hello").rfind(u'l') == 3u);
  static_assert(u16cstring_view(u"hello").rfind(u'l', 2u) == 2u);

  static_assert(u32cstring_view(U"hello").rfind(U'e') == 1u);
  static_assert(u32cstring_view(U"hello").rfind(U'z') == cstring_view::npos);
  static_assert(u32cstring_view(U"hello").rfind(U'l') == 3u);
  static_assert(u32cstring_view(U"hello").rfind(U'l', 2u) == 2u);

#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").rfind(L'e') == 1u);
  static_assert(wcstring_view(L"hello").rfind(L'z') == cstring_view::npos);
  static_assert(wcstring_view(L"hello").rfind(L'l') == 3u);
  static_assert(wcstring_view(L"hello").rfind(L'l', 2u) == 2u);
#endif

  // Searching for a string.
  static_assert(cstring_view("hello hello").rfind("lo") == 9u);
  static_assert(cstring_view("hello hello").rfind("lol") == cstring_view::npos);
  static_assert(u16cstring_view(u"hello hello").rfind(u"lo") == 9u);
  static_assert(u16cstring_view(u"hello hello").rfind(u"lol") ==
                cstring_view::npos);
  static_assert(u32cstring_view(U"hello hello").rfind(U"lo") == 9u);
  static_assert(u32cstring_view(U"hello hello").rfind(U"lol") ==
                cstring_view::npos);
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello hello").rfind(L"lo") == 9u);
  static_assert(wcstring_view(L"hello hello").rfind(L"lol") ==
                cstring_view::npos);
#endif
}

TEST(CStringViewTest, FindFirstOf) {
  // OOB `pos` will return npos. The NUL is never searched.
  static_assert(cstring_view("hello").find_first_of('h', 1000u) ==
                cstring_view::npos);
  static_assert(cstring_view("hello").find_first_of('\0', 5u) ==
                cstring_view::npos);

  // Searching for a Char.
  static_assert(cstring_view("hello").find_first_of('e') == 1u);
  static_assert(cstring_view("hello").find_first_of('z') == cstring_view::npos);
  static_assert(cstring_view("hello").find_first_of('l') == 2u);
  static_assert(cstring_view("hello").find_first_of('l', 3u) == 3u);

  static_assert(u16cstring_view(u"hello").find_first_of(u'e') == 1u);
  static_assert(u16cstring_view(u"hello").find_first_of(u'z') ==
                cstring_view::npos);
  static_assert(u16cstring_view(u"hello").find_first_of(u'l') == 2u);
  static_assert(u16cstring_view(u"hello").find_first_of(u'l', 3u) == 3u);

  static_assert(u32cstring_view(U"hello").find_first_of(U'e') == 1u);
  static_assert(u32cstring_view(U"hello").find_first_of(U'z') ==
                cstring_view::npos);
  static_assert(u32cstring_view(U"hello").find_first_of(U'l') == 2u);
  static_assert(u32cstring_view(U"hello").find_first_of(U'l', 3u) == 3u);

#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").find_first_of(L'e') == 1u);
  static_assert(wcstring_view(L"hello").find_first_of(L'z') ==
                cstring_view::npos);
  static_assert(wcstring_view(L"hello").find_first_of(L'l') == 2u);
  static_assert(wcstring_view(L"hello").find_first_of(L'l', 3u) == 3u);
#endif

  // Searching for a string.
  static_assert(cstring_view("hello hello").find_first_of("ol") == 2u);
  static_assert(cstring_view("hello hello").find_first_of("zz") ==
                cstring_view::npos);
  static_assert(u16cstring_view(u"hello hello").find_first_of(u"ol") == 2u);
  static_assert(u16cstring_view(u"hello hello").find_first_of(u"zz") ==
                cstring_view::npos);
  static_assert(u32cstring_view(U"hello hello").find_first_of(U"ol") == 2u);
  static_assert(u32cstring_view(U"hello hello").find_first_of(U"zz") ==
                cstring_view::npos);
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello hello").find_first_of(L"ol") == 2u);
  static_assert(wcstring_view(L"hello hello").find_first_of(L"zz") ==
                cstring_view::npos);
#endif
}

TEST(CStringViewTest, FindLastOf) {
  // OOB `pos` will clamp to the end of the view. The NUL is never searched.
  static_assert(cstring_view("hello").find_last_of('h', 0u) == 0u);
  static_assert(cstring_view("hello").find_last_of('h', 1000u) == 0u);
  static_assert(cstring_view("hello").find_last_of('\0', 5u) ==
                cstring_view::npos);

  // Searching for a Char.
  static_assert(cstring_view("hello").find_last_of('e') == 1u);
  static_assert(cstring_view("hello").find_last_of('z') == cstring_view::npos);
  static_assert(cstring_view("hello").find_last_of('l') == 3u);
  static_assert(cstring_view("hello").find_last_of('l', 2u) == 2u);

  static_assert(u16cstring_view(u"hello").find_last_of(u'e') == 1u);
  static_assert(u16cstring_view(u"hello").find_last_of(u'z') ==
                cstring_view::npos);
  static_assert(u16cstring_view(u"hello").find_last_of(u'l') == 3u);
  static_assert(u16cstring_view(u"hello").find_last_of(u'l', 2u) == 2u);

  static_assert(u32cstring_view(U"hello").find_last_of(U'e') == 1u);
  static_assert(u32cstring_view(U"hello").find_last_of(U'z') ==
                cstring_view::npos);
  static_assert(u32cstring_view(U"hello").find_last_of(U'l') == 3u);
  static_assert(u32cstring_view(U"hello").find_last_of(U'l', 2u) == 2u);

#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").find_last_of(L'e') == 1u);
  static_assert(wcstring_view(L"hello").find_last_of(L'z') ==
                cstring_view::npos);
  static_assert(wcstring_view(L"hello").find_last_of(L'l') == 3u);
  static_assert(wcstring_view(L"hello").find_last_of(L'l', 2u) == 2u);
#endif

  // Searching for a string.
  static_assert(cstring_view("hello hello").find_last_of("lo") == 10u);
  static_assert(cstring_view("hello hello").find_last_of("zz") ==
                cstring_view::npos);
  static_assert(u16cstring_view(u"hello hello").find_last_of(u"lo") == 10u);
  static_assert(u16cstring_view(u"hello hello").find_last_of(u"zz") ==
                cstring_view::npos);
  static_assert(u32cstring_view(U"hello hello").find_last_of(U"lo") == 10u);
  static_assert(u32cstring_view(U"hello hello").find_last_of(U"zz") ==
                cstring_view::npos);
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello hello").find_last_of(L"lo") == 10u);
  static_assert(wcstring_view(L"hello hello").find_last_of(L"zz") ==
                cstring_view::npos);
#endif
}

TEST(CStringViewTest, FindFirstNotOf) {
  // OOB `pos` will return npos. The NUL is never searched.
  static_assert(cstring_view("hello").find_first_not_of('a', 1000u) ==
                cstring_view::npos);
  static_assert(cstring_view("hello").find_first_not_of('a', 5u) ==
                cstring_view::npos);

  // Searching for a Char.
  static_assert(cstring_view("hello").find_first_not_of('h') == 1u);
  static_assert(cstring_view("hello").find_first_not_of('e') == 0u);
  static_assert(cstring_view("hello").find_first_not_of("eloh") ==
                cstring_view::npos);

  static_assert(u16cstring_view(u"hello").find_first_not_of(u'h') == 1u);
  static_assert(u16cstring_view(u"hello").find_first_not_of(u'e') == 0u);
  static_assert(u16cstring_view(u"hello").find_first_not_of(u"eloh") ==
                cstring_view::npos);

  static_assert(u32cstring_view(U"hello").find_first_not_of(U'h') == 1u);
  static_assert(u32cstring_view(U"hello").find_first_not_of(U'e') == 0u);
  static_assert(u32cstring_view(U"hello").find_first_not_of(U"eloh") ==
                cstring_view::npos);

#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").find_first_not_of(L'h') == 1u);
  static_assert(wcstring_view(L"hello").find_first_not_of(L'e') == 0u);
  static_assert(wcstring_view(L"hello").find_first_not_of(L"eloh") ==
                cstring_view::npos);
#endif

  // Searching for a string.
  static_assert(cstring_view("hello hello").find_first_not_of("eh") == 2u);
  static_assert(cstring_view("hello hello").find_first_not_of("hello ") ==
                cstring_view::npos);
  static_assert(u16cstring_view(u"hello hello").find_first_not_of(u"eh") == 2u);
  static_assert(u16cstring_view(u"hello hello").find_first_not_of(u"hello ") ==
                cstring_view::npos);
  static_assert(u32cstring_view(U"hello hello").find_first_not_of(U"eh") == 2u);
  static_assert(u32cstring_view(U"hello hello").find_first_not_of(U"hello ") ==
                cstring_view::npos);
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello hello").find_first_not_of(L"eh") == 2u);
  static_assert(wcstring_view(L"hello hello").find_first_not_of(L"hello ") ==
                cstring_view::npos);
#endif
}

TEST(CStringViewTest, FindLastNotOf) {
  // OOB `pos` will clamp to the end of the view. The NUL is never searched.
  static_assert(cstring_view("hello").find_last_not_of('a', 1000u) == 4u);
  static_assert(cstring_view("hello").find_last_not_of('a', 5u) == 4u);

  // Searching for a Char.
  static_assert(cstring_view("hello").find_last_not_of('l') == 4u);
  static_assert(cstring_view("hello").find_last_not_of('o') == 3u);
  static_assert(cstring_view("hello").find_last_not_of("eloh") ==
                cstring_view::npos);

  static_assert(u16cstring_view(u"hello").find_last_not_of(u'l') == 4u);
  static_assert(u16cstring_view(u"hello").find_last_not_of(u'o') == 3u);
  static_assert(u16cstring_view(u"hello").find_last_not_of(u"eloh") ==
                cstring_view::npos);

  static_assert(u32cstring_view(U"hello").find_last_not_of(U'l') == 4u);
  static_assert(u32cstring_view(U"hello").find_last_not_of(U'o') == 3u);
  static_assert(u32cstring_view(U"hello").find_last_not_of(U"eloh") ==
                cstring_view::npos);

#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello").find_last_not_of(L'l') == 4u);
  static_assert(wcstring_view(L"hello").find_last_not_of(L'o') == 3u);
  static_assert(wcstring_view(L"hello").find_last_not_of(L"eloh") ==
                cstring_view::npos);
#endif

  // Searching for a string.
  static_assert(cstring_view("hello hello").find_last_not_of("lo") == 7u);
  static_assert(cstring_view("hello hello").find_last_not_of("hello ") ==
                cstring_view::npos);
  static_assert(u16cstring_view(u"hello hello").find_last_not_of(u"lo") == 7u);
  static_assert(u16cstring_view(u"hello hello").find_last_not_of(u"hello ") ==
                cstring_view::npos);
  static_assert(u32cstring_view(U"hello hello").find_last_not_of(U"lo") == 7u);
  static_assert(u32cstring_view(U"hello hello").find_last_not_of(U"hello ") ==
                cstring_view::npos);
#if BUILDFLAG(IS_WIN)
  static_assert(wcstring_view(L"hello hello").find_last_not_of(L"lo") == 7u);
  static_assert(wcstring_view(L"hello hello").find_last_not_of(L"hello ") ==
                cstring_view::npos);
#endif
}

TEST(CStringViewTest, ToString) {
  // Streaming support like std::string_view.
  std::ostringstream s;
  s << cstring_view("hello");
  EXPECT_EQ(s.str(), "hello");

#if BUILDFLAG(IS_WIN)
  std::wostringstream sw;
  sw << wcstring_view(L"hello");
  EXPECT_EQ(sw.str(), L"hello");
#endif

  // Gtest printing support.
  EXPECT_EQ(testing::PrintToString(cstring_view("hello")), "hello");
}

TEST(CStringViewTest, Hash) {
  [[maybe_unused]] auto s = std::hash<cstring_view>()(cstring_view("hello"));
  static_assert(std::same_as<size_t, decltype(s)>);

  [[maybe_unused]] auto s16 =
      std::hash<u16cstring_view>()(u16cstring_view(u"hello"));
  static_assert(std::same_as<size_t, decltype(s)>);

  [[maybe_unused]] auto s32 =
      std::hash<u32cstring_view>()(u32cstring_view(U"hello"));
  static_assert(std::same_as<size_t, decltype(s)>);

#if BUILDFLAG(IS_WIN)
  [[maybe_unused]] auto sw =
      std::hash<wcstring_view>()(wcstring_view(L"hello"));
  static_assert(std::same_as<size_t, decltype(s)>);
#endif
}

TEST(CStringViewTest, IntoStdStringView) {
  // string_view implicitly constructs from const char*, and so also from
  // cstring_view.
  std::string_view sc = "hello";
  std::string_view s = cstring_view("hello");
  EXPECT_EQ(s, sc);

  static_assert(std::string_view(cstring_view("hello")) == "hello");
}

TEST(CStringViewTest, IntoStdString) {
  // string implicitly constructs from const char*, but not from
  // std::string_view or cstring_view.
  static_assert(std::convertible_to<const char*, std::string>);
  static_assert(!std::convertible_to<std::string_view, std::string>);
  static_assert(!std::convertible_to<cstring_view, std::string>);

  static_assert(std::constructible_from<std::string, const char*>);
  static_assert(std::constructible_from<std::string, std::string_view>);
  static_assert(std::constructible_from<std::string, cstring_view>);

  auto sv = std::string(std::string_view("hello"));
  auto cs = std::string(cstring_view("hello"));
  EXPECT_EQ(cs, sv);

  static_assert(std::string(cstring_view("hello")) == "hello");
}

TEST(CStringViewTest, StringPlus) {
  {
    auto s = cstring_view("hello") + std::string("world");
    static_assert(std::same_as<std::string, decltype(s)>);
    EXPECT_EQ(s, "helloworld");
  }
  {
    auto s = std::string("hello") + cstring_view("world");
    static_assert(std::same_as<std::string, decltype(s)>);
    EXPECT_EQ(s, "helloworld");
  }
  {
    auto s = std::u16string(u"hello") + u16cstring_view(u"world");
    static_assert(std::same_as<std::u16string, decltype(s)>);
    EXPECT_EQ(s, u"helloworld");
  }
  {
    auto s = std::u32string(U"hello") + u32cstring_view(U"world");
    static_assert(std::same_as<std::u32string, decltype(s)>);
    EXPECT_EQ(s, U"helloworld");
  }
  {
#if BUILDFLAG(IS_WIN)
    auto s = std::wstring(L"hello") + wcstring_view(L"world");
    static_assert(std::same_as<std::wstring, decltype(s)>);
    EXPECT_EQ(s, L"helloworld");
#endif
  }

  // From lvalues.
  {
    auto h = cstring_view("hello");
    auto w = std::string("world");
    auto s = h + w;
    static_assert(std::same_as<std::string, decltype(s)>);
    EXPECT_EQ(s, "helloworld");
  }

  static_assert(cstring_view("hello") + std::string("world") == "helloworld");
  static_assert(std::string("hello") + cstring_view("world") == "helloworld");
}

TEST(CStringViewTest, StringAppend) {
  std::string s = "hello";
  // string::append() can accept cstring_view like const char*.
  s.append(cstring_view("world"));
  EXPECT_EQ(s, "helloworld");
}

TEST(CStringViewTest, StringInsert) {
  std::string s = "world";
  // string::insert() can accept cstring_view like const char*.
  s.insert(0u, cstring_view("hello"));
  EXPECT_EQ(s, "helloworld");
}

TEST(CStringViewTest, StringReplace) {
  std::string s = "goodbyeworld";
  // string::replace() can accept cstring_view like const char*.
  s.replace(0u, 7u, cstring_view("hello"));
  EXPECT_EQ(s, "helloworld");
}

TEST(CStringViewTest, StringFind) {
  const std::string s = "helloworld";
  // string::find() can accept cstring_view like const char*.
  EXPECT_EQ(s.find(cstring_view("owo")), 4u);
}

TEST(CStringViewTest, StringCompare) {
  const std::string s = "hello";
  // string::compare() can accept cstring_view like const char*.
  EXPECT_EQ(s.compare(cstring_view("hello")), 0);
  // string::operator== can accept cstring_view like const char*.
  EXPECT_EQ(s, cstring_view("hello"));
  // string::operator<=> can accept cstring_view like const char*.
  EXPECT_EQ(s <=> cstring_view("hello"), std::weak_ordering::equivalent);
  // string::operator<= etc can accept cstring_view like const char*. This
  // follows from <=> normally but std::string has more overloads.
  EXPECT_LE(s, cstring_view("hello"));
}

TEST(CStringViewTest, StringStartsEndsWith) {
  const std::string s = "hello";
  // string::starts_with() can accept cstring_view like const char*.
  EXPECT_EQ(s.starts_with(cstring_view("hel")), true);
  EXPECT_EQ(s.starts_with(cstring_view("lo")), false);
  // string::ends_with() can accept cstring_view like const char*.
  EXPECT_EQ(s.ends_with(cstring_view("hel")), false);
  EXPECT_EQ(s.ends_with(cstring_view("lo")), true);
}

TEST(CStringViewTest, StrCat) {
  EXPECT_EQ(base::StrCat({cstring_view("hello"), std::string_view("world")}),
            "helloworld");
}

TEST(CStringViewTest, Example_CtorLiteral) {
  const char kLiteral[] = "hello world";
  auto s = base::cstring_view(kLiteral);
  CHECK(s == "hello world");
  auto s2 = base::cstring_view("this works too");
  CHECK(s2 == "this works too");
}

TEST(CStringViewTest, CompatibleWithRanges) {
  EXPECT_EQ(2, ranges::count(cstring_view("hello"), 'l'));
}

TEST(CStringViewTest, ConstructFromStringLiteralWithEmbeddedNul) {
  const std::string s = "abc\0de";
  constexpr std::string_view sv = "abc\0de";
  constexpr base::cstring_view cv = "abc\0de";
  EXPECT_EQ(s, std::string_view("abc"));
  EXPECT_EQ(sv, std::string_view("abc"));
  EXPECT_EQ(cv, std::string_view("abc"));

  constexpr base::u16cstring_view cv16 = u"abc\0de";
  EXPECT_EQ(cv16, std::u16string_view(u"abc"));
  constexpr base::u32cstring_view cv32 = U"abc\0de";
  EXPECT_EQ(cv32, std::u32string_view(U"abc"));
#if BUILDFLAG(IS_WIN)
  constexpr base::wcstring_view cvw = L"abc\0de";
  EXPECT_EQ(cvw, std::wstring_view(L"abc"));
#endif
}

}  // namespace
}  // namespace base
