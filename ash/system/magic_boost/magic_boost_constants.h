// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_
#define ASH_SYSTEM_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_

namespace ash::magic_boost {

// Specifies which features are the opt in ones.
enum class OptInFeatures {
  // Default option. Only opt in HMR.
  kHmrOnly,
  // Opt in both Orca and HMR features.
  kOrcaAndHmr,
};

// Specifies which action to complete after the opt-in flow has been
// completed, transitioning from the opt-in experience to the main feature.
enum class TransitionAction {
  // Default option. Not do anything.
  kDoNothing,
  // Show the editor panel after completing the opt-in flow.
  kShowEditorPanel,
  // Show the Mahi panel after completing the opt-in flow.
  kShowHmrPanel,
  // Show the Lobster UI after completing the opt-in flow.
  kShowLobsterPanel,
};

// The view ids for Magic Boost related views.
enum ViewId {
  DisclaimerViewAcceptButton = 1,
  DisclaimerViewDeclineButton,
  DisclaimerViewParagraphOne,
  DisclaimerViewParagraphTwo,
  DisclaimerViewParagraphThree,
  DisclaimerViewParagraphFour,
  DisclaimerViewTitle,
};

}  // namespace ash::magic_boost

#endif  // ASH_SYSTEM_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_
