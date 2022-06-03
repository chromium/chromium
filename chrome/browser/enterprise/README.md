## About //chrome/browser/enterprise/

This directory is used for enterprise or edu related features and util
functions.

## Creating new sub-directory
  * Each new feature needs to be in its own sub-directory.
  * Each new feature needs to be in its own namespace that begins with
    `enterprise_`.
  * Sub-directory should be owned by the feature owners.

## What does not belong here
  * Code that does not belong to `//chrome/browser/`.
  * Code that is related to policy loading and applying. It should be put into
    `//components/policy/` or `//chrome/browser/(chromeos/)policy/`.
  * Code that fits in a more narrow context. This includes most of the policy
    implementations.

## Responsibilities of //chrome/browser/enterprise/OWNERS
  * Reviewing new features.
  * Reviewing large scale refactoring.
  * Maintaining util functions that don't have owners.
