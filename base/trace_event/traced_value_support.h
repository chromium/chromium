// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACED_VALUE_SUPPORT_H_
#define BASE_TRACE_EVENT_TRACED_VALUE_SUPPORT_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

// This file contains specialisations for trace serialisation for key
// widely-used //base classes. As these specialisations require full definition
// of perfetto::TracedValue and almost every source unit in Chromium requires
// one of these //base concepts, include specialiazations here and expose them
// to the users including trace_event.h, rather than adding a dependency from
// scoped_refptr.h et al on traced_value.h.

namespace perfetto {

// If T is serialisable into a trace, scoped_refptr<T> is serialisable as well.
template <class T>
struct TraceFormatTraits<scoped_refptr<T>,
                         perfetto::check_traced_value_support_t<T>> {
  static void WriteIntoTracedValue(perfetto::TracedValue context,
                                   const scoped_refptr<T>& value) {
    if (!value) {
      std::move(context).WritePointer(nullptr);
      return;
    }
    perfetto::WriteIntoTracedValue(std::move(context), *value);
  }
};

// If T is serialisable into a trace, base::WeakPtr<T> is serialisable as well.
template <class T>
struct TraceFormatTraits<::base::WeakPtr<T>,
                         perfetto::check_traced_value_support_t<T>> {
  static void WriteIntoTracedValue(perfetto::TracedValue context,
                                   const ::base::WeakPtr<T>& value) {
    if (!value) {
      std::move(context).WritePointer(nullptr);
      return;
    }
    perfetto::WriteIntoTracedValue(std::move(context), *value);
  }
};

// If T is serialisable into a trace, base::Optional<T> is serialisable as well.
// Note that we need definitions for both base::Optional<T>& and
// const base::Optional<T>& (unlike scoped_refptr and WeakPtr above), as
// dereferencing const scoped_refptr<T>& gives you T, while dereferencing const
// base::Optional<T>& gives you const T&.
template <class T>
struct TraceFormatTraits<::base::Optional<T>,
                         perfetto::check_traced_value_support_t<T>> {
  static void WriteIntoTracedValue(perfetto::TracedValue context,
                                   const ::base::Optional<T>& value) {
    if (!value) {
      std::move(context).WritePointer(nullptr);
      return;
    }
    perfetto::WriteIntoTracedValue(std::move(context), *value);
  }

  static void WriteIntoTracedValue(perfetto::TracedValue context,
                                   ::base::Optional<T>& value) {
    if (!value) {
      std::move(context).WritePointer(nullptr);
      return;
    }
    perfetto::WriteIntoTracedValue(std::move(context), *value);
  }
};

// Time-related classes.
// TODO(altimin): Make them first-class primitives in TracedValue and Perfetto
// UI.
template <>
struct TraceFormatTraits<::base::TimeDelta> {
  static void WriteIntoTracedValue(perfetto::TracedValue context,
                                   const ::base::TimeDelta& value) {
    std::move(context).WriteUInt64(value.InMicroseconds());
  }
};

template <>
struct TraceFormatTraits<::base::TimeTicks> {
  static void WriteIntoTracedValue(perfetto::TracedValue context,
                                   const ::base::TimeTicks& value) {
    perfetto::WriteIntoTracedValue(std::move(context), value.since_origin());
  }
};

template <>
struct TraceFormatTraits<::base::Time> {
  static void WriteIntoTracedValue(perfetto::TracedValue context,
                                   const ::base::Time& value) {
    perfetto::WriteIntoTracedValue(std::move(context), value.since_origin());
  }
};

// base::UnguessableToken.
// TODO(altimin): Add first-class primitive, which will allow to show a
// human-comprehensible alias for all unguessable tokens instead.
template <>
struct TraceFormatTraits<::base::UnguessableToken> {
  static void WriteIntoTracedValue(perfetto::TracedValue context,
                                   const ::base::UnguessableToken& value) {
    return std::move(context).WriteString(value.ToString());
  }
};

}  // namespace perfetto

#endif  // BASE_TRACE_EVENT_TRACED_VALUE_SUPPORT_H_
