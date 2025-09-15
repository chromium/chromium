# Chromium AI Coding Policy

This policy is for Chromium developers using AI tools to help write code. It is
intended to outline expectations around the use of such tools.

## Responsibilities

Authors **must** self-review and understand all code and documentation updates
(with or without AI tooling) before sending them for review to ensure the
correctness, design, security properties, and style meet the standards of the
project. Authors should be able to answer questions reviewers have about the
changes. Beyond code quality, Chromium has a strict 2-committer code review
requirement and when the author is a committer they are considered one of the
two human reviewers. **Any account that sends for review CLs which are not
actually understood by the human behind the account is at risk of losing their
committer status. Further violations after being warned may result in the
account being banned from the system.**

To aid reviewers, authors **should** flag areas that they are not confident
about that had AI assistance. This can be done in code review comments, the CL
description, or in code comments. There is a precedent for separating
automatically-generated code from manual edits with different patchsets (e.g.
patchset 1 has automatic changes and the reproduction instructions and patchset
2+ have manual edits) along with steps to reproduce the automated parts.

Authors **must** attest that the code they submit is their original creation,
regardless of whether AI tooling was used.

## Recommendations

Authors **may** explain in the CL description or the code base itself how AI
tools were used to produce the CL.

Examples:

*   If a single prompt to a tool (e.g. gemini-cli) was used to create the CL
    then the prompt may be included in the CL description.
*   If a design spec was provided along with a prompt as input to a tool that
    produced a working change, the spec may be checked in alongside the code and
    the prompt may be included in the CL description.

Additional examples for gemini-cli can be added to `//agents/prompts/eval`,
which will serve as eval cases for improvements to common system prompts.

## Google Employees

See go/chrome-internal-ai-policy for additional requirements.
