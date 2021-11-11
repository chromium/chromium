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
struct EnumTraits<ash::ime::mojom::SuggestionMode,
                  ash::ime::TextSuggestionMode> {
  using TextSuggestionMode = ::ash::ime::TextSuggestionMode;
  using SuggestionMode = ::ash::ime::mojom::SuggestionMode;

  static SuggestionMode ToMojom(TextSuggestionMode mode);
  static bool FromMojom(SuggestionMode input, TextSuggestionMode* output);
};

template <>
struct EnumTraits<ash::ime::mojom::SuggestionType,
                  ash::ime::TextSuggestionType> {
  using TextSuggestionType = ::ash::ime::TextSuggestionType;
  using SuggestionType = ::ash::ime::mojom::SuggestionType;

  static SuggestionType ToMojom(TextSuggestionType type);
  static bool FromMojom(SuggestionType input, TextSuggestionType* output);
};

template <>
struct StructTraits<ash::ime::mojom::SuggestionCandidateDataView,
                    ash::ime::TextSuggestion> {
  using SuggestionCandidateDataView =
      ::ash::ime::mojom::SuggestionCandidateDataView;
  using TextSuggestion = ::ash::ime::TextSuggestion;
  using TextSuggestionMode = ::ash::ime::TextSuggestionMode;
  using TextSuggestionType = ::ash::ime::TextSuggestionType;

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
struct StructTraits<ash::ime::mojom::CompletionCandidateDataView,
                    ash::ime::TextCompletionCandidate> {
  using CompletionCandidateDataView =
      ::ash::ime::mojom::CompletionCandidateDataView;
  using TextCompletionCandidate = ::ash::ime::TextCompletionCandidate;

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
struct StructTraits<ash::ime::mojom::TextRangeDataView, gfx::Range> {
  static uint32_t start(const gfx::Range& r) { return r.start(); }
  static uint32_t end(const gfx::Range& r) { return r.end(); }
  static bool Read(ash::ime::mojom::TextRangeDataView data, gfx::Range* out) {
    out->set_start(data.start());
    out->set_end(data.end());
    return true;
  }
};

}  // namespace mojo

#endif  // ASH_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_
