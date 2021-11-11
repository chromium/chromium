// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_
#define ASH_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_

#include "ash/services/ime/public/cpp/suggestions.h"
#include "ash/services/ime/public/mojom/input_method_host.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/range/range.h"

namespace mojo {

template <>
struct EnumTraits<chromeos::ime::mojom::SuggestionMode,
                  chromeos::ime::TextSuggestionMode> {
  using TextSuggestionMode = ::chromeos::ime::TextSuggestionMode;
  using SuggestionMode = ::chromeos::ime::mojom::SuggestionMode;

  static SuggestionMode ToMojom(TextSuggestionMode mode);
  static bool FromMojom(SuggestionMode input, TextSuggestionMode* output);
};

template <>
struct EnumTraits<chromeos::ime::mojom::SuggestionType,
                  chromeos::ime::TextSuggestionType> {
  using TextSuggestionType = ::chromeos::ime::TextSuggestionType;
  using SuggestionType = ::chromeos::ime::mojom::SuggestionType;

  static SuggestionType ToMojom(TextSuggestionType type);
  static bool FromMojom(SuggestionType input, TextSuggestionType* output);
};

template <>
struct StructTraits<chromeos::ime::mojom::SuggestionCandidateDataView,
                    chromeos::ime::TextSuggestion> {
  using SuggestionCandidateDataView =
      ::chromeos::ime::mojom::SuggestionCandidateDataView;
  using TextSuggestion = ::chromeos::ime::TextSuggestion;
  using TextSuggestionMode = ::chromeos::ime::TextSuggestionMode;
  using TextSuggestionType = ::chromeos::ime::TextSuggestionType;

  static TextSuggestionMode mode(const TextSuggestion& suggestion) {
    return suggestion.mode;
  }
  static TextSuggestionType type(const TextSuggestion& suggestion) {
    return suggestion.type;
  }
  static const std::string& text(const TextSuggestion& suggestion) {
    return suggestion.text;
  }

  static bool Read(SuggestionCandidateDataView input, TextSuggestion* output);
};

template <>
struct StructTraits<chromeos::ime::mojom::CompletionCandidateDataView,
                    chromeos::ime::TextCompletionCandidate> {
  using CompletionCandidateDataView =
      ::chromeos::ime::mojom::CompletionCandidateDataView;
  using TextCompletionCandidate = ::chromeos::ime::TextCompletionCandidate;

  static const std::string& text(const TextCompletionCandidate& candidate) {
    return candidate.text;
  }

  static const float& normalized_score(
      const TextCompletionCandidate& candidate) {
    return candidate.score;
  }

  static bool Read(CompletionCandidateDataView input,
                   TextCompletionCandidate* output);
};

template <>
struct StructTraits<chromeos::ime::mojom::TextRangeDataView, gfx::Range> {
  static uint32_t start(const gfx::Range& r) { return r.start(); }
  static uint32_t end(const gfx::Range& r) { return r.end(); }
  static bool Read(chromeos::ime::mojom::TextRangeDataView data,
                   gfx::Range* out) {
    out->set_start(data.start());
    out->set_end(data.end());
    return true;
  }
};

}  // namespace mojo

#endif  // ASH_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_
