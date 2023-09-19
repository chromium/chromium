// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MOVING_WINDOW_H_
#define BASE_MOVING_WINDOW_H_

#include <stddef.h>

#include <functional>
#include <limits>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ref.h"

namespace base {

// Class to efficiently calculate statistics in a sliding window.
// This class isn't thread safe.
// Supported statistics are Min/Max/Mean/Variance/Deviation.
// You can also iterate through the items in the window.
// The class is modular: required features must be specified in the template
// arguments.
// Non listed features don't consume memory or runtime cycles at all.
//
// Usage:
// base::MovingWindow<int,
//                    base::MovingWindowFeatures::Min,
//                    base::MovingWindowFeatures::Max>
//                    moving_window(window_size);
//
// Following convenience shortcuts are provided with predefined sets of
// features: MovingMax/MovingMin/MovingMean/MovingMeanVariance/MovingMinMax.
//
// Methods:
// Constructor:
//   MovingWindow(size_t window_size);
//
// Window update (available for all templates):
//   AddSample(T value) const;
//   size_t Count() const;
//   void Reset();
//
// Available for MovingWindowFeatures::Min:
//    T Min() const;
//
// Available for MovingWindowFeatures::Max:
//    T Max() const;
//
// Available for MovingWindowFeatures::Mean:
//    U Mean<U>() const;
//
// Available for MovingWindowFeatures::Variance:
//    T Variance() const;
//    double Deviation() const;
//
// Available for MovingWindowFeatures::Iteration. Iterating through the window:
//    iterator begin() const;
//    iterator begin() const;
//    size_t size() const;

// Features supported by the class.
struct MovingWindowFeatures {
  struct Min {
    static bool has_min;
  };

  struct Max {
    static bool has_max;
  };

  // Need to specify a type capable of holding a sum of all elements in the
  // window.
  template <typename SumType>
  struct Mean {
    static SumType has_mean;
  };

  // Need to specify a type capable of holding a sum of squares of all elements
  // in the window.
  template <typename SumType>
  struct Variance {
    static SumType has_variance;
  };

  struct Iteration {
    static bool has_iteration;
  };
};

// Main template.
template <typename T, typename... Features>
class MovingWindow;

// Convenience shortcuts.
template <typename T>
using MovingMax = MovingWindow<T, MovingWindowFeatures::Max>;

template <typename T>
using MovingMin = MovingWindow<T, MovingWindowFeatures::Min>;

template <typename T>
using MovingMinMax =
    MovingWindow<T, MovingWindowFeatures::Min, MovingWindowFeatures::Max>;

// TODO(crbug.com/1475160): Rename to MovingAverage once the old MovingAverage
// is removed.
template <typename T, typename SumType>
using MovingMean = MovingWindow<T, MovingWindowFeatures::Mean<SumType>>;

template <typename T, typename SumType, typename SumSquareType>
using MovingMeanVariance =
    MovingWindow<T,
                 MovingWindowFeatures::Mean<SumType>,
                 MovingWindowFeatures::Variance<SumSquareType>>;

namespace internal {

// Class responsible only for calculating maximum in the window.
// It's reused to calculate both min and max via inverting the comparator.
template <typename T, typename Comparator>
class MovingExtremumBase {
 public:
  explicit MovingExtremumBase(size_t window_size)
      : window_size_(window_size),
        values_(window_size),
        added_at_(window_size),
        last_idx_(window_size - 1),
        compare_(Comparator()) {}
  ~MovingExtremumBase() = default;

  // Add new sample to the stream.
  void AddSample(const T& value, size_t& total_added) {
    // Remove old elements from the back of the window;
    while (size_ > 0 && added_at_[begin_idx_] + window_size_ <= total_added) {
      ++begin_idx_;
      if (begin_idx_ == window_size_) {
        begin_idx_ = 0;
      }
      --size_;
    }
    // Remove small elements from the front of the window because they can never
    // become the maximum in the window since the currently added element is
    // bigger than them and will leave the window later.
    while (size_ > 0 && compare_(values_[last_idx_], value)) {
      if (last_idx_ == 0) {
        last_idx_ = window_size_;
      }
      --last_idx_;
      --size_;
    }
    DCHECK_LT(size_, window_size_);
    ++last_idx_;
    if (last_idx_ == window_size_) {
      last_idx_ = 0;
    }
    values_[last_idx_] = value;
    added_at_[last_idx_] = total_added;
    ++size_;
  }

  // Get the maximum of the last `window_size` elements.
  T Value() const {
    DCHECK_GT(size_, 0u);
    return values_[begin_idx_];
  }

  // Clear all samples.
  void Reset() {
    size_ = 0;
    begin_idx_ = 0;
    last_idx_ = window_size_ - 1;
  }

 private:
  const size_t window_size_;
  // Circular buffer with some values in the window.
  // Only possible candidates for maximum are stored:
  // values form a non-increasing sequence.
  std::vector<T> values_;
  // Circular buffer storing when numbers in `values_` were added.
  std::vector<size_t> added_at_;
  // Begin of the circular buffers above.
  size_t begin_idx_ = 0;
  // Last occupied position.
  size_t last_idx_;
  // How many elements are stored in the circular buffers above.
  size_t size_ = 0;
  // Template parameter comparator.
  const Comparator compare_;
};

// Null implementation of the above class to be used when feature is disabled.
template <typename T>
struct NullExtremumImpl {
  explicit NullExtremumImpl(size_t) {}
  ~NullExtremumImpl() = default;
  void AddSample(const T&, size_t&) {}
  void Reset() {}
};

// Class to hold the moving window.
// It's used to calculate replaced element for Mean/Variance calculations.
template <typename T>
class MovingWindowBase {
 public:
  explicit MovingWindowBase(size_t window_size) : values_(window_size) {}

  ~MovingWindowBase() = default;

  void AddSample(const T& sample) {
    values_[cur_idx_] = sample;
    ++cur_idx_;
    if (cur_idx_ == values_.size()) {
      cur_idx_ = 0;
    }
  }

  // Is the window filled integer amount of times.
  bool IsLastIdx() const { return cur_idx_ + 1 == values_.size(); }

  void Reset() {
    cur_idx_ = 0;
    std::fill(values_.begin(), values_.end(), T());
  }

  T GetValue() const { return values_[cur_idx_]; }

  T operator[](size_t idx) const { return values_[idx]; }

  size_t Size() const { return values_.size(); }

  // What index will be overwritten by a new element;
  size_t CurIdx() const { return cur_idx_; }

 private:
  // Circular buffer.
  std::vector<T> values_;
  // Where the buffer begins.
  size_t cur_idx_ = 0;
};

// Null implementation of the above class to be used when feature is disabled.
template <typename T>
struct NullWindowImpl {
  explicit NullWindowImpl(size_t) {}
  ~NullWindowImpl() = default;
  void AddSample(const T& sample) {}
  bool IsLastIdx() const { return false; }
  void Reset() {}
  T GetValue() const { return T(); }
};

// Class to calculate moving mean.
template <typename T, typename SumType, bool IsFloating>
class MovingMeanBase {
 public:
  explicit MovingMeanBase(size_t window_size) : sum_() {}

  ~MovingMeanBase() = default;

  void AddSample(const T& sample, const T& replaced_value, bool is_last_idx) {
    sum_ += sample - replaced_value;
  }

  template <typename ReturnType = SumType>
  ReturnType Mean(const size_t& count) const {
    if (count == 0) {
      return ReturnType();
    }
    return static_cast<ReturnType>(sum_) / static_cast<ReturnType>(count);
  }
  void Reset() { sum_ = SumType(); }

  SumType Sum() const { return sum_; }

 private:
  SumType sum_;
};

// Class to calculate moving mean.
// Variant for float types with running sum to avoid rounding errors
// accumulation.
template <typename T, typename SumType>
class MovingMeanBase<T, SumType, true> {
 public:
  explicit MovingMeanBase(size_t window_size) : sum_(), running_sum_() {}

  ~MovingMeanBase() = default;

  void AddSample(const T& sample, const T& replaced_value, bool is_last_idx) {
    running_sum_ += sample;
    if (is_last_idx) {
      // Replace sum with running sum to avoid rounding errors accumulation.
      sum_ = running_sum_;
      running_sum_ = SumType();
    } else {
      sum_ += sample - replaced_value;
    }
  }

  template <typename ReturnType = SumType>
  ReturnType Mean(const size_t& count) const {
    if (count == 0) {
      return ReturnType();
    }
    return static_cast<ReturnType>(sum_) / static_cast<ReturnType>(count);
  }

  void Reset() { sum_ = running_sum_ = SumType(); }

  SumType Sum() const { return sum_; }

 private:
  SumType sum_;
  SumType running_sum_;
};

// Null implementation of the above class to be used when feature is disabled.
template <typename T>
struct NullMeanImpl {
  explicit NullMeanImpl(size_t window_size) {}
  ~NullMeanImpl() = default;

  void AddSample(const T& sample, const T&, bool) {}

  void Reset() {}
};

// Class to calculate moving variance.
template <typename T, typename SumType, bool IsFloating>
class MovingVarianceBase {
 public:
  explicit MovingVarianceBase(size_t window_size) : sum_sq_() {}
  ~MovingVarianceBase() = default;
  void AddSample(const T& sample, const T& replaced_value, bool is_last_idx) {
    sum_sq_ += sample * static_cast<T>(sample) -
               replaced_value * static_cast<T>(replaced_value);
  }

  T Variance(const size_t& count, const SumType& sum) const {
    if (count == 0) {
      return SumType();
    }
    // Variance is equal to mean of squared values minus squared mean value.
    SumType squared_sum = sum * sum;
    return (sum_sq_ - squared_sum / static_cast<SumType>(count)) /
           static_cast<SumType>(count);
  }
  void Reset() { sum_sq_ = SumType(); }

 private:
  SumType sum_sq_;
};

// Class to calculate moving variance.
// Variant for float types with running sum to avoid rounding errors
// accumulation.
template <typename T, typename SumType>
class MovingVarianceBase<T, SumType, true> {
 public:
  explicit MovingVarianceBase(size_t window_size) : sum_sq_(), running_sum_() {}
  ~MovingVarianceBase() = default;
  void AddSample(const T& sample, const T& replaced_value, bool is_last_idx) {
    SumType square = sample * static_cast<T>(sample);
    running_sum_ += square;
    if (is_last_idx) {
      // Replace sum with running sum to avoid rounding errors accumulation.
      sum_sq_ = running_sum_;
      running_sum_ = SumType();
    } else {
      sum_sq_ += square - replaced_value * static_cast<T>(replaced_value);
    }
  }
  T Variance(const size_t& count, const SumType& sum) const {
    if (count == 0) {
      return SumType();
    }
    // Variance is equal to mean of squared values minus squared mean value.
    SumType squared_sum = sum * sum;
    return (sum_sq_ - squared_sum / static_cast<SumType>(count)) /
           static_cast<SumType>(count);
  }
  void Reset() { running_sum_ = sum_sq_ = SumType(); }

 private:
  SumType sum_sq_;
  SumType running_sum_;
};

// Null implementation of the above class to be used when feature is disabled.
template <typename T>
struct NullVarianceImpl {
 public:
  explicit NullVarianceImpl(size_t window_size) {}
  ~NullVarianceImpl() = default;
  void AddSample(const T&, const T&, bool) {}
  void Reset() {}
};

// Template helpers.

// Gets all enabled features in one struct.
template <typename... Features>
struct EnabledFeatures : public Features... {};

// Checks if specific member is present.
template <typename T, typename = void>
struct has_member_min : std::false_type {};
template <typename T>
struct has_member_min<T, decltype((void)T::has_min, void())> : std::true_type {
};

template <typename T, typename = void>
struct has_member_max : std::false_type {};
template <typename T>
struct has_member_max<T, decltype((void)T::has_max, void())> : std::true_type {
};

template <typename T, typename = void>
struct has_member_mean : std::false_type {};
template <typename T>
struct has_member_mean<T, decltype((void)T::has_mean, void())>
    : std::true_type {};

template <typename T, typename = void>
struct has_memeber_variance : std::false_type {};
template <typename T>
struct has_memeber_variance<T, decltype((void)T::has_variance, void())>
    : std::true_type {};

template <typename T, typename = void>
struct has_member_iteration : std::false_type {};
template <typename T>
struct has_member_iteration<T, decltype((void)T::has_iteration, void())>
    : std::true_type {};

// Gets the type of the member if present.
// Can't just use decltype, because the member might be absent.
template <typename T, typename = void>
struct get_type_mean {
  typedef void type;
};
template <typename T>
struct get_type_mean<T, decltype((void)T::has_mean, void())> {
  typedef decltype(T::has_mean) type;
};

template <typename T, typename = void>
struct get_type_variance {
  typedef void type;
};
template <typename T>
struct get_type_variance<T, decltype((void)T::has_variance, void())> {
  typedef decltype(T::has_variance) type;
};
}  // namespace internal

// Implementation of the main class.
template <typename T, typename... Features>
class MovingWindow {
 public:
  // List of all requested features.
  using EnabledFeatures = internal::EnabledFeatures<Features...>;

  explicit MovingWindow(size_t window_size)
      : min_impl_(window_size),
        max_impl_(window_size),
        mean_impl_(window_size),
        variance_impl_(window_size),
        window_impl_(window_size) {}

  // Adds sample to the window.
  void AddSample(T sample) {
    ++total_added_;
    min_impl_.AddSample(sample, total_added_);
    max_impl_.AddSample(sample, total_added_);
    mean_impl_.AddSample(sample, window_impl_.GetValue(),
                         window_impl_.IsLastIdx());
    variance_impl_.AddSample(sample, window_impl_.GetValue(),
                             window_impl_.IsLastIdx());
    window_impl_.AddSample(sample);
  }

  // Returns amount of elementes so far in the stream (might be bigger than the
  // window size).
  size_t Count() const { return total_added_; }

  // Calculates min in the window. Template to disable when feature isn't
  // requested.
  template <typename U = EnabledFeatures,
            typename std::enable_if<internal::has_member_min<U>::value,
                                    int>::type = 0>
  T Min() const {
    return min_impl_.Value();
  }

  // Calculates max in the window. Template to disable when feature isn't
  // requested.
  template <typename U = EnabledFeatures,
            typename std::enable_if<internal::has_member_max<U>::value,
                                    int>::type = 0>
  T Max() const {
    return max_impl_.Value();
  }

  // Calculates mean in the window. Template to disable when feature isn't
  // requested.
  template <typename ReturnType = T,
            typename U = EnabledFeatures,
            typename std::enable_if<internal::has_member_mean<U>::value,
                                    int>::type = 0>
  ReturnType Mean() const {
    return mean_impl_.template Mean<ReturnType>(
        std::min(total_added_, window_impl_.Size()));
  }

  // Calculates variance in the window. Template to disable when feature isn't
  // requested.
  template <typename U = EnabledFeatures,
            typename std::enable_if<internal::has_memeber_variance<U>::value,
                                    int>::type = 0>
  T Variance() const {
    const size_t count = std::min(total_added_, window_impl_.Size());
    return variance_impl_.Variance(count, mean_impl_.Sum());
  }

  // Calculates standard deviation in the window. Template to disable when
  // Variance feature isn't requested.
  template <typename U = EnabledFeatures,
            typename std::enable_if<internal::has_memeber_variance<U>::value,
                                    int>::type = 0>
  double Deviation() const {
    const size_t count = std::min(total_added_, window_impl_.Size());
    return sqrt(
        static_cast<double>(variance_impl_.Variance(count, mean_impl_.Sum())));
  }

  // Resets the state to an empty window.
  void Reset() {
    min_impl_.Reset();
    max_impl_.Reset();
    mean_impl_.Reset();
    variance_impl_.Reset();
    window_impl_.Reset();
    total_added_ = 0;
  }

  // iterator implementation.
  class iterator {
   public:
    ~iterator() = default;

    const T operator*() {
      DCHECK_LT(idx_, window_impl_->Size());
      return (*window_impl_)[idx_];
    }

    iterator& operator++() {
      ++idx_;
      // Wrap around the circular buffer.
      if (idx_ == window_impl_->Size()) {
        idx_ = 0;
      }
      // The only way to arrive to the current element is to
      // come around after iterating through the whole window.
      if (idx_ == window_impl_->CurIdx()) {
        idx_ = kInvalidIndex;
      }
      return *this;
    }

    bool operator==(const iterator& other) const { return idx_ == other.idx_; }

   private:
    iterator(const internal::MovingWindowBase<T>& window, size_t idx)
        : window_impl_(window), idx_(idx) {}

    static const size_t kInvalidIndex = std::numeric_limits<size_t>::max();

    raw_ref<const internal::MovingWindowBase<T>> window_impl_;
    size_t idx_;

    friend class MovingWindow<T, Features...>;
  };

  // Begin iterator. Template to enable only if iteration feature is requested.
  template <typename U = EnabledFeatures,
            typename std::enable_if<internal::has_member_iteration<U>::value,
                                    int>::type = 0>
  iterator begin() const {
    if (total_added_ == 0) {
      return end();
    }
    // Before window is fully filled, the oldest element is at the index 0.
    size_t idx =
        (total_added_ < window_impl_.Size()) ? 0 : window_impl_.CurIdx();

    return iterator(window_impl_, idx);
  }

  // End iterator. Template to enable only if iteration feature is requested.
  template <typename U = EnabledFeatures,
            typename std::enable_if<internal::has_member_iteration<U>::value,
                                    int>::type = 0>
  iterator end() const {
    return iterator(window_impl_, iterator::kInvalidIndex);
  }

  // Size of the collection. Template to enable only if iteration feature is
  // requested.
  template <typename U = EnabledFeatures,
            typename std::enable_if<internal::has_member_iteration<U>::value,
                                    int>::type = 0>
  size_t size() const {
    return std::min(total_added_, window_impl_.Size());
  }

 private:
  // Member for calculating min.
  // Conditionally enabled on Min feature.
  typename std::conditional<internal::has_member_min<EnabledFeatures>::value,
                            internal::MovingExtremumBase<T, std::greater<T>>,
                            internal::NullExtremumImpl<T>>::type min_impl_;

  // Member for calculating min.
  // Conditionally enabled on Min feature.
  typename std::conditional<internal::has_member_max<EnabledFeatures>::value,
                            internal::MovingExtremumBase<T, std::less<T>>,
                            internal::NullExtremumImpl<T>>::type max_impl_;

  // Type for sum value in Mean implementation. Might need to reuse variance sum
  // type, because enabling only variance feature will also enable mean member
  // (because variance calculation depends on mean calculation).
  using MeanSumType = typename std::conditional<
      internal::has_member_mean<EnabledFeatures>::value,
      typename internal::get_type_mean<EnabledFeatures>::type,
      typename internal::get_type_variance<EnabledFeatures>::type>::type;
  // Member for calculating mean.
  // Conditionally enabled on Mean or Variance feature (because variance
  // calculation depends on mean calculation).
  typename std::conditional<
      internal::has_member_mean<EnabledFeatures>::value ||
          internal::has_memeber_variance<EnabledFeatures>::value,
      internal::MovingMeanBase<T,
                               MeanSumType,
                               std::is_floating_point<MeanSumType>::value>,
      internal::NullMeanImpl<T>>::type mean_impl_;

  // Member for calculating variance.
  // Conditionally enabled on Variance feature.
  typename std::conditional<
      internal::has_memeber_variance<EnabledFeatures>::value,
      internal::MovingVarianceBase<
          T,
          typename internal::get_type_variance<EnabledFeatures>::type,
          std::is_floating_point<typename internal::get_type_variance<
              EnabledFeatures>::type>::value>,
      internal::NullVarianceImpl<T>>::type variance_impl_;

  // Member for storing the moving window.
  // Conditionally enabled on Mean, Variance or Iteration feature since
  // they need the elements in the window.
  // Min and Max features store elements internally so they don't need this.
  typename std::conditional<
      internal::has_member_mean<EnabledFeatures>::value ||
          internal::has_memeber_variance<EnabledFeatures>::value ||
          internal::has_member_iteration<EnabledFeatures>::value,
      internal::MovingWindowBase<T>,
      internal::NullWindowImpl<T>>::type window_impl_;
  // Total number of added elements.
  size_t total_added_ = 0;
};

}  // namespace base

#endif  // BASE_MOVING_WINDOW_H_
