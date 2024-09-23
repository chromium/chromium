# Link-Preview

## Overview
The
[Link-Preview](https://docs.google.com/document/d/1hrWfVIDrPkrBlf8A576dDBH7Q34ESMLvOObt0j9i0SU/edit?usp=sharing)
feature allows users to quickly and safely preview linked pages in a small
window. While the user views the preview, Chrome actively prevents intrusive
behaviors, similar to the
[Prerendering](https://wicg.github.io/nav-speculation/prerendering.html)
feature, to ensure a secure preview experience. The preview window is
intentionally non-interactive (e.g., users cannot input text into a form or
click links) but can be promoted to a normal tab by clicking on it.

## Current Status
In the initial phase, we are conducting experiments with three different trigger
mechanisms (Alt + Click, Alt + Hover, Long Press) to determine the optimal
balance between usability and safety.

## Experiments
### Enable the Feature
Users can manually enable the feature through chrome://flags/#link-preview.

### Experiment Groups
1. Alt + Click trigger: Preview is triggered by pressing Alt and clicking a
link.
2. Alt + Hover trigger: Preview is triggered by pressing Alt and hovering over
a link.
3. Long Press trigger: Preview is triggered by pressing and holding a link.

## Feedback
We are
[gathering anonymous usage data](https://chromium.googlesource.com/chromium/src/+/main/tools/metrics/histograms/README.md)
during these experiments to assess the effectiveness of each trigger mechanism.
You don't need to provide explicit feedback on your preferred method, but if you
encounter any issues, please report them through
[this](https://crbug.com/343110535) crbug link.
