---
name: components/register-iph-features-in-actions-and-histograms-xml
rubric: testing
severity: should_fix
---

**Rule.** When introducing a new In-Product Help (IPH) feature in
components/feature_engagement, register its action variant under <variants
name="InProductHelp_Type"> in tools/metrics/actions/actions.xml (with 'IPH'
stripped and '\_' prepended) and its histogram variant under <variants
name="IPHFeature"> in
tools/metrics/histograms/metadata/feature_engagement/histograms.xml.
Alternatively, run python3 tools/feature_engagement/generate_iph_entry.py to
generate all required UMA boilerplate.

**Why.** Feature Engagement automatically records user actions and histograms
(such as InProductHelp.NotifyEvent.IPH, InProductHelp.ShouldTriggerHelpUI.IPH,
and InProductHelp.Summary) parameterized by feature name. If variants are
omitted in actions.xml and histograms.xml, UMA telemetry cannot properly
recognize, aggregate, or process metrics for the new IPH feature.

## Examples

Bad:

```cpp
// In components/feature_engagement/public/feature_constants.cc:
BASE_FEATURE(kIPHSidePanelContextualTasksPinnableFeature,
             "IPH_SidePanelContextualTasksPinnableFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Missing corresponding action and histogram variants in metrics XML files
```

Good:

```cpp
<!-- In tools/metrics/actions/actions.xml under <variants name="InProductHelp_Type">: -->
<variant name="_SidePanelContextualTasksPinnableFeature"
    summary="contextual tasks side panel pinnable"/>

<!-- In tools/metrics/histograms/metadata/feature_engagement/histograms.xml under <variants name="IPHFeature">: -->
<variant name="IPH_SidePanelContextualTasksPinnableFeature"
    summary="contextual tasks side panel pinnable"/>
```
