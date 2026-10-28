# Allowed Metadata Files (safe_dirs.md)

This skill may ONLY discover, analyze, bundle, or modify histograms whose
`<histogram>` definition lives in the allowlisted `histograms.xml` files listed
below. No other `histograms.xml` under
`tools/metrics/histograms/metadata/<dir>/` may be queried (by
`scripts/find_candidates.py`, `scripts/find_expired_in_file.py`,
`rg`/`cs`-driven discovery, or manual reading), proposed, bundled as co-located,
or edited.

- **Last Audited:** 2026-10-26 (tracked under
  [crbug.com/499059525](https://crbug.com/499059525))
- **Re-validation Cadence:** Re-validate this list at least every 6 months (or
  before adding any new `metadata/<dir>/histograms.xml` entry) to verify that
  expired histograms in these directories remain safe for automated cleanup.

> [!IMPORTANT] The discovery scripts parse this file directly as the single
> source of truth. Keep one backticked path per bullet in the exact
> `tools/metrics/histograms/metadata/<dir>/histograms.xml` form, and never add
> any other literal path of that shape anywhere in this file.

## Allowlist

- `tools/metrics/histograms/metadata/blink/histograms.xml`
- `tools/metrics/histograms/metadata/media/histograms.xml`
- `tools/metrics/histograms/metadata/navigation/histograms.xml`
- `tools/metrics/histograms/metadata/service/histograms.xml`
- `tools/metrics/histograms/metadata/extensions/histograms.xml`
- `tools/metrics/histograms/metadata/mobile/histograms.xml`
- `tools/metrics/histograms/metadata/cras/histograms.xml`
- `tools/metrics/histograms/metadata/structured_metrics/histograms.xml`
- `tools/metrics/histograms/metadata/download/histograms.xml`
- `tools/metrics/histograms/metadata/web_core/histograms.xml`
- `tools/metrics/histograms/metadata/custom_tabs/histograms.xml`
- `tools/metrics/histograms/metadata/local/histograms.xml`
- `tools/metrics/histograms/metadata/omnibox/histograms.xml`
- `tools/metrics/histograms/metadata/android/histograms.xml`
- `tools/metrics/histograms/metadata/prefetch/histograms.xml`
- `tools/metrics/histograms/metadata/stability/histograms.xml`
- `tools/metrics/histograms/metadata/uma/histograms.xml`
- `tools/metrics/histograms/metadata/cookie/histograms.xml`
- `tools/metrics/histograms/metadata/web_apk/histograms.xml`
- `tools/metrics/histograms/metadata/visited_url_ranking/histograms.xml`
- `tools/metrics/histograms/metadata/content_settings/histograms.xml`
- `tools/metrics/histograms/metadata/gcm/histograms.xml`
- `tools/metrics/histograms/metadata/lens/histograms.xml`
- `tools/metrics/histograms/metadata/search/histograms.xml`
- `tools/metrics/histograms/metadata/enterprise/histograms.xml`
- `tools/metrics/histograms/metadata/feature_engagement/histograms.xml`
- `tools/metrics/histograms/metadata/settings/histograms.xml`
- `tools/metrics/histograms/metadata/startup/histograms.xml`
- `tools/metrics/histograms/metadata/ui/histograms.xml`
- `tools/metrics/histograms/metadata/preloading/histograms.xml`
- `tools/metrics/histograms/metadata/renderer4/histograms.xml`
- `tools/metrics/histograms/metadata/background/histograms.xml`
- `tools/metrics/histograms/metadata/bookmarks/histograms.xml`
- `tools/metrics/histograms/metadata/chrome/histograms.xml`
- `tools/metrics/histograms/metadata/chromeos_hps/histograms.xml`
- `tools/metrics/histograms/metadata/compositing/histograms.xml`
- `tools/metrics/histograms/metadata/mac/histograms.xml`
- `tools/metrics/histograms/metadata/notifications/histograms.xml`
- `tools/metrics/histograms/metadata/offline/histograms.xml`
- `tools/metrics/histograms/metadata/scheduler/histograms.xml`
- `tools/metrics/histograms/metadata/variations/histograms.xml`
- `tools/metrics/histograms/metadata/dev/histograms.xml`
- `tools/metrics/histograms/metadata/event/histograms.xml`
- `tools/metrics/histograms/metadata/storage/histograms.xml`
- `tools/metrics/histograms/metadata/data/histograms.xml`
- `tools/metrics/histograms/metadata/game_mode/histograms.xml`
- `tools/metrics/histograms/metadata/language/histograms.xml`
- `tools/metrics/histograms/metadata/network/histograms.xml`
- `tools/metrics/histograms/metadata/permissions/histograms.xml`
- `tools/metrics/histograms/metadata/privacy/histograms.xml`
- `tools/metrics/histograms/metadata/safe_browsing/histograms.xml`
- `tools/metrics/histograms/metadata/session/histograms.xml`
- `tools/metrics/histograms/metadata/signin/histograms.xml`
- `tools/metrics/histograms/metadata/borealis/histograms.xml`
- `tools/metrics/histograms/metadata/cros/histograms.xml`
- `tools/metrics/histograms/metadata/interstitial/histograms.xml`
- `tools/metrics/histograms/metadata/invalidation/histograms.xml`
- `tools/metrics/histograms/metadata/leveldb_proto/histograms.xml`
- `tools/metrics/histograms/metadata/linux/histograms.xml`
- `tools/metrics/histograms/metadata/login/histograms.xml`
- `tools/metrics/histograms/metadata/on_device_model/histograms.xml`
- `tools/metrics/histograms/metadata/payment/histograms.xml`
- `tools/metrics/histograms/metadata/subresource/histograms.xml`
- `tools/metrics/histograms/metadata/system/histograms.xml`
- `tools/metrics/histograms/metadata/translate/histograms.xml`
- `tools/metrics/histograms/metadata/trusted_vault/histograms.xml`
- `tools/metrics/histograms/metadata/update_engine/histograms.xml`
- `tools/metrics/histograms/metadata/user_education/histograms.xml`

## Rules

- **Non-allowlisted definition:** Any histogram whose `<histogram>` definition
  is outside the files listed above receives Confidence Score = 0 immediately
  and must not be analyzed or removed.
- **Co-located / bundled histograms:** Every co-located or bundled histogram
  must pass the allowlist check independently; any member defined outside this
  list must be dropped from the bundle and left untouched in code.
- **Automated review:** The automated review step returns `FAIL` if a patch
  modifies any `histograms.xml` file not listed above.
