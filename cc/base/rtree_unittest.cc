// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/rtree.h"

#include <stddef.h>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {
// Helper function to use in place of rtree. Search that ensures that every
// call to Search / SearchRefs produces the same results.
template <typename T>
void SearchAndVerifyRefs(const RTree<T>& rtree,
                         const gfx::Rect& query,
                         std::vector<T>* results) {
  rtree.Search(query, results);

  // Perform the same query with SearchRefs and make sure it matches Search.
  std::vector<const T*> ref_results;
  rtree.SearchRefs(query, &ref_results);
  ASSERT_EQ(ref_results.size(), results->size());
  for (size_t i = 0; i < results->size(); ++i) {
    EXPECT_EQ(*ref_results[i], (*results)[i]);
  }
}

template <typename T>
void SearchAndVerifyBounds(const RTree<T>& rtree,
                           const gfx::Rect& query,
                           std::vector<T>* results,
                           std::vector<gfx::Rect>* rects) {
  rtree.Search(query, results, rects);
  ASSERT_EQ(results->size(), rects->size());
  for (auto& rect : *rects) {
    EXPECT_TRUE(rect.Intersects(query));
  }
}
}  // namespace

TEST(RTreeTest, ReserveNodesDoesntDcheck) {
  // Make sure that anywhere between 0 and 1000 rects, our reserve math in rtree
  // is correct. (This test would DCHECK if broken either in
  // RTree::AllocateNodeAtLevel, indicating that the capacity calculation was
  // too small or in RTree::Build, indicating the capacity was too large).
  for (int i = 0; i < 1000; ++i) {
    std::vector<gfx::Rect> rects;
    for (int j = 0; j < i; ++j)
      rects.push_back(gfx::Rect(j, i, 1, 1));
    RTree<size_t> rtree;
    rtree.Build(rects);
  }
}

TEST(RTreeTest, NoOverlap) {
  std::vector<gfx::Rect> rects;
  for (int y = 0; y < 50; ++y) {
    for (int x = 0; x < 50; ++x) {
      rects.push_back(gfx::Rect(x, y, 1, 1));
    }
  }

  RTree<size_t> rtree;
  rtree.Build(rects);

  std::vector<size_t> results;
  SearchAndVerifyRefs(rtree, gfx::Rect(0, 0, 50, 50), &results);
  ASSERT_EQ(2500u, results.size());
  // Note that the results have to be sorted.
  for (size_t i = 0; i < 2500; ++i) {
    ASSERT_EQ(results[i], i);
  }

  SearchAndVerifyRefs(rtree, gfx::Rect(0, 0, 50, 49), &results);
  ASSERT_EQ(2450u, results.size());
  for (size_t i = 0; i < 2450; ++i) {
    ASSERT_EQ(results[i], i);
  }

  SearchAndVerifyRefs(rtree, gfx::Rect(5, 6, 1, 1), &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(6u * 50 + 5u, results[0]);
}

TEST(RTreeTest, Overlap) {
  std::vector<gfx::Rect> rects;
  for (int h = 1; h <= 50; ++h) {
    for (int w = 1; w <= 50; ++w) {
      rects.push_back(gfx::Rect(0, 0, w, h));
    }
  }

  RTree<size_t> rtree;
  rtree.Build(rects);

  std::vector<size_t> results;
  SearchAndVerifyRefs(rtree, gfx::Rect(0, 0, 1, 1), &results);
  ASSERT_EQ(2500u, results.size());
  // Both the checks for the elements assume elements are sorted.
  for (size_t i = 0; i < 2500; ++i) {
    ASSERT_EQ(results[i], i);
  }

  SearchAndVerifyRefs(rtree, gfx::Rect(0, 49, 1, 1), &results);
  ASSERT_EQ(50u, results.size());
  for (size_t i = 0; i < 50; ++i) {
    EXPECT_EQ(results[i], 2450u + i);
  }
}

static void VerifySorted(const std::vector<size_t>& results) {
  for (size_t i = 1; i < results.size(); ++i) {
    ASSERT_LT(results[i - 1], results[i]);
  }
}

TEST(RTreeTest, SortedResults) {
  // This test verifies that all queries return sorted elements.
  std::vector<gfx::Rect> rects;
  for (int y = 0; y < 50; ++y) {
    for (int x = 0; x < 50; ++x) {
      rects.push_back(gfx::Rect(x, y, 1, 1));
      rects.push_back(gfx::Rect(x, y, 2, 2));
      rects.push_back(gfx::Rect(x, y, 3, 3));
    }
  }

  RTree<size_t> rtree;
  rtree.Build(rects);

  for (int y = 0; y < 50; ++y) {
    for (int x = 0; x < 50; ++x) {
      std::vector<size_t> results;
      SearchAndVerifyRefs(rtree, gfx::Rect(x, y, 1, 1), &results);
      VerifySorted(results);
      SearchAndVerifyRefs(rtree, gfx::Rect(x, y, 50, 1), &results);
      VerifySorted(results);
      SearchAndVerifyRefs(rtree, gfx::Rect(x, y, 1, 50), &results);
      VerifySorted(results);
    }
  }
}

TEST(RTreeTest, GetBoundsEmpty) {
  RTree<size_t> rtree;
  EXPECT_EQ(gfx::Rect(), *rtree.bounds());
  EXPECT_TRUE(rtree.GetAllBoundsForTracing().empty());
}

TEST(RTreeTest, GetBoundsNonOverlapping) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(5, 6, 7, 8));
  rects.push_back(gfx::Rect(11, 12, 13, 14));

  RTree<size_t> rtree;
  rtree.Build(rects);

  EXPECT_EQ(gfx::Rect(5, 6, 19, 20), *rtree.bounds());
  std::map<size_t, gfx::Rect> expected_all_bounds = {{0, rects[0]},
                                                     {1, rects[1]}};
  EXPECT_EQ(expected_all_bounds, rtree.GetAllBoundsForTracing());
}

TEST(RTreeTest, GetBoundsOverlapping) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 10, 10));
  rects.push_back(gfx::Rect(5, 5, 5, 5));

  RTree<size_t> rtree;
  rtree.Build(rects);

  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), *rtree.bounds());
  std::map<size_t, gfx::Rect> expected_all_bounds = {{0, rects[0]},
                                                     {1, rects[1]}};
  EXPECT_EQ(expected_all_bounds, rtree.GetAllBoundsForTracing());
}

TEST(RTreeTest, GetBoundsWithEmptyRect) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect());
  rects.push_back(gfx::Rect(5, 5, 5, 5));

  RTree<size_t> rtree;
  rtree.Build(rects);

  EXPECT_EQ(gfx::Rect(5, 5, 5, 5), *rtree.bounds());
  std::map<size_t, gfx::Rect> expected_all_bounds = {{1, rects[1]}};
  EXPECT_EQ(expected_all_bounds, rtree.GetAllBoundsForTracing());
}

TEST(RTreeTest, Payload) {
  using Container = std::vector<std::pair<gfx::Rect, float>>;
  Container data;
  data.emplace_back(gfx::Rect(10, 10, 10, 10), 40.f);
  data.emplace_back(gfx::Rect(0, 0, 10, 10), 10.f);
  data.emplace_back(gfx::Rect(0, 10, 10, 10), 30.f);
  data.emplace_back(gfx::Rect(10, 0, 10, 10), 20.f);

  RTree<float> rtree;
  rtree.Build(
      data.size(), [&data](size_t index) { return data[index].first; },
      [&data](size_t index) { return data[index].second; });

  std::vector<float> results;
  SearchAndVerifyRefs(rtree, gfx::Rect(0, 0, 1, 1), &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_FLOAT_EQ(10.f, results[0]);

  // Search with bounds
  std::vector<gfx::Rect> rects;
  SearchAndVerifyBounds(rtree, gfx::Rect(0, 0, 1, 1), &results, &rects);
  ASSERT_EQ(1u, results.size());
  ASSERT_EQ(results.size(), rects.size());
  EXPECT_FLOAT_EQ(10.f, results[0]);
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), rects[0]);

  SearchAndVerifyRefs(rtree, gfx::Rect(5, 5, 10, 10), &results);
  ASSERT_EQ(4u, results.size());
  // Items returned should be in the order they were inserted.
  EXPECT_FLOAT_EQ(40.f, results[0]);
  EXPECT_FLOAT_EQ(10.f, results[1]);
  EXPECT_FLOAT_EQ(30.f, results[2]);
  EXPECT_FLOAT_EQ(20.f, results[3]);
}

TEST(RTreeTest, InvalidBounds) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(-INT_MAX, -INT_MAX, INT_MAX, INT_MAX));
  rects.push_back(gfx::Rect(100, 100, 10, 10));

  RTree<size_t> rtree;
  rtree.Build(rects);

  EXPECT_FALSE(rtree.has_valid_bounds());
}

TEST(RTreeTest, InvalidBoundsSearch) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(-INT_MAX, -INT_MAX, INT_MAX, INT_MAX));
  rects.push_back(gfx::Rect(100, 100, 10, 10));
  rects.push_back(gfx::Rect(105, 105, 10, 10));
  rects.push_back(gfx::Rect(-50, -50, 10, 10));
  rects.push_back(gfx::Rect(INT_MAX - 100, INT_MAX - 100, 10, 10));

  RTree<size_t> rtree;
  rtree.Build(rects);

  EXPECT_FALSE(rtree.has_valid_bounds());

  // Searching should still work.
  std::vector<size_t> found;
  SearchAndVerifyRefs(rtree, gfx::Rect(0, 0, INT_MAX, INT_MAX), &found);
  EXPECT_EQ(found, std::vector<size_t>({1, 2, 4}));
  SearchAndVerifyRefs(rtree, gfx::Rect(-INT_MAX, -INT_MAX, INT_MAX, INT_MAX),
                      &found);
  EXPECT_EQ(found, std::vector<size_t>({0, 3}));
  SearchAndVerifyRefs(rtree, gfx::Rect(-50, -50, INT_MAX, INT_MAX), &found);
  EXPECT_EQ(found, std::vector<size_t>({0, 1, 2, 3, 4}));
}

TEST(RTreeTest, InvalidBoundsGetAllBounds) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(-INT_MAX, -INT_MAX, INT_MAX, INT_MAX));
  rects.push_back(gfx::Rect(100, 100, 10, 10));
  rects.push_back(gfx::Rect(105, 105, 10, 10));
  rects.push_back(gfx::Rect(-50, -50, 10, 10));
  rects.push_back(gfx::Rect(INT_MAX - 100, INT_MAX - 100, 10, 10));

  RTree<size_t> rtree;
  rtree.Build(rects);

  EXPECT_FALSE(rtree.has_valid_bounds());

  // Getting all bounds should still work.
  std::map<size_t, gfx::Rect> all_bounds = rtree.GetAllBoundsForTracing();
  std::map<size_t, gfx::Rect> expected_all_bounds = {{0, rects[0]},
                                                     {1, rects[1]},
                                                     {2, rects[2]},
                                                     {3, rects[3]},
                                                     {4, rects[4]}};
  EXPECT_EQ(all_bounds, expected_all_bounds);
}

}  // namespace cc
