// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_PREFS_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_PREFS_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace welcome_tour_metrics {
enum class ExperimentalArm;
enum class Interaction;
enum class PreventedReason;
}  // namespace welcome_tour_metrics

namespace welcome_tour_prefs {

// Retrieves the experimental arm in which the user was active when the first
// attempt was made to show the Welcome Tour. If the value has not been set,
// returns `std::nullopt`.
ASH_EXPORT std::optional<welcome_tour_metrics::ExperimentalArm>
GetFirstExperimentalArm(PrefService* prefs);

// Retrieves the time that the given `interaction` first occurred after the
// tour. If the time has not been set, returns `std::nullopt`.
ASH_EXPORT std::optional<base::Time> GetTimeOfFirstInteraction(
    PrefService* prefs,
    welcome_tour_metrics::Interaction interaction);

// Retrieves the time that the tour was first aborted. If the time has not
// been set, returns `std::nullopt`.
ASH_EXPORT std::optional<base::Time> GetTimeOfFirstTourAborted(
    PrefService* prefs);

// Retrieves the time that the tour was first attempted. If the time has not
// been set, returns `std::nullopt`.
ASH_EXPORT std::optional<base::Time> GetTimeOfFirstTourAttempt(
    PrefService* prefs);

// Retrieves the time that the tour was first completed. If the time has not
// been set, returns `std::nullopt`.
ASH_EXPORT std::optional<base::Time> GetTimeOfFirstTourCompletion(
    PrefService* prefs);

// Retrieves the time that the tour was first prevented. If the time has not
// been set, returns `std::nullopt`.
ASH_EXPORT std::optional<base::Time> GetTimeOfFirstTourPrevention(
    PrefService* prefs);

// Retrieves the reason the tour was first prevented. If the tour has not been
// prevented, returns `std::nullopt`.
ASH_EXPORT std::optional<welcome_tour_metrics::PreventedReason>
GetReasonForFirstTourPrevention(PrefService* prefs);

// Marks the experimental arm in which the user was active when the first
// attempt was made to show the Welcome Tour. Returns true if it was
// successfully marked.
ASH_EXPORT bool MarkFirstExperimentalArm(
    PrefService* prefs,
    welcome_tour_metrics::ExperimentalArm arm);

// Marks now as the first time the tour was prevented, with the given `reason`.
// Returns true if it was successfully marked.
ASH_EXPORT bool MarkFirstTourPrevention(
    PrefService* prefs,
    welcome_tour_metrics::PreventedReason reason);

// Marks now as the first time that a given `interaction` has occurred. Returns
// true if it was successfully marked.
ASH_EXPORT bool MarkTimeOfFirstInteraction(
    PrefService* prefs,
    welcome_tour_metrics::Interaction interaction);

// Marks now as the first time the tour was aborted. Returns true if it was
// successfully marked.
ASH_EXPORT bool MarkTimeOfFirstTourAborted(PrefService* prefs);

// Marks now as the first time the tour was completed. Returns true if it was
// successfully marked.
ASH_EXPORT bool MarkTimeOfFirstTourCompletion(PrefService* prefs);

// Registers the Welcome Tour prefs to the given `registry`.
ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace welcome_tour_prefs
}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_PREFS_H_
