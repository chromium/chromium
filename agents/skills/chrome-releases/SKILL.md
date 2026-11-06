---
name: chrome-releases
description: >-
  Maps Git commits, version tags, channel releases, and milestone schedules. Use
  when finding first landed versions of commits/CLs, channel releases, milestone
  branches/dates, version containment, or commit ranges. Don't use for UMA/crash
  metrics, installing Chrome, CI builders, bugs, or code diffs.
---

# Chrome Releases & Commit Tracker

Queries commits, versions, releases, and milestones via `chrome_releases.py`
(Developer Graph API).

## Capabilities & Commands

### 1. `commit`

```bash
python3 agents/skills/chrome-releases/scripts/chrome_releases.py commit {commit_hash}
```

- **Argument**: 40-char lowercase SHA from `chromium` (`chromium/src`, `v8/v8`,
  `angle/angle`, `devtools/devtools-frontend`), `skia` (`skia`), `webrtc`
  (`src`), `dawn` (`dawn`), `pdfium` (`pdfium`), or `boringssl` (`boringssl`);
  unindexed repos like `chrome-internal`, `ios_internal`, and `chromiumos/*`
  return HTTP 404—use their `chromium/src` `DEPS` roll commit.

- **Resolve Gerrit CLs or short SHAs first** (`{query}` is `change:{cl_or_id}`
  or `commit:{short_sha}`, or use `git rev-parse {short_sha}` locally):

  ```bash
  curl -s \
    "https://{host}-review.googlesource.com/changes/?q={query}&o=CURRENT_REVISION" \
    | tail -n +2
  ```

  Check `status`, `branch`, and `current_revision` in the JSON response.

- **Response fields**: `chromeMetadata.commitPosition`,
  `chromeMetadata.versions` (earliest `chrome` version tag, including for
  rolled-in sub-repo commits), `sourceCommits` / `derivedCommits`
  (`DEPENDENCY_ADD`, `DEPENDENCY_REMOVE`), `gerritChanges`.

### 2. `version`

```bash
python3 agents/skills/chrome-releases/scripts/chrome_releases.py version {version}
```

- **Argument**: Version string (e.g. `136.0.7051.0`; only default
  `--product chrome` is supported—`v8` returns HTTP 404) or alias:
  `latest-main`, `latest-{branch}` (e.g. `latest-7103`),
  `latest-{branch}_{minibranch}` (e.g. `latest-7258_13`).
- **Response fields**:
  - `commitHash`: Git tag commit SHA.
  - `chromeMetadata.parentVersion`: Preceding version.
  - `chromeMetadata.minibranch`: Patch number on parent branch if on a
    minibranch.
  - `chromeMetadata.firstReleases`: First release per `{platform}/{channel}`
    with `version` and `startTime` (resolves trunk versions across milestone
    branch cuts). Full single-line JSON (~15 KB) may truncate; for specific
    `{platform}/{channel}` lookups, pipe
    `| grep -o '"name":"chrome/platforms/{platform}/channels/{channel}/[^}]*}' || true`.

### 3. `milestone`

```bash
python3 agents/skills/chrome-releases/scripts/chrome_releases.py milestone {milestone_number}
```

- **Response fields**: `branches` (e.g. `refs/branch-heads/6723` and divergence
  `commitHash`), `events` (`branch_point`, `feature_freeze`, `earliest_beta`,
  `final_beta_cut`, `final_beta`, `stable_cut`, `early_stable`, `stable_date`,
  `late_stable_date`, `stable_refresh_first`/`second`/`third`, `chromeos_*`).

## Workflows

1. **Commit/CL Landed Version, Release, Finch & Soak**: Resolve any CL or short
   SHA, run `commit`, and read the `chrome` entry in `chromeMetadata.versions`
   (trunk Finch `min_version`; sub-repo commits already emit this rolled-in
   `chrome` version directly—skip running `commit` on `derivedCommits`). For
   channel releases, run `version` on `{chrome_version}` and read
   `firstReleases` (check `{platform}/canary` `startTime` for 24h merge soak).

2. **Version Containment (`A` in `B`, `Major.Minor.Build.Patch`)**: Get `A` from
   `commit` (`chromeMetadata.versions`). If `A.Patch == 0`, check
   `A.Build <= B.Build`. If `A.Patch > 0` and `A.Build == B.Build`, run
   `version` on both `A` and `B` for `chromeMetadata.minibranch` (`0` if unset;
   returned as a string, so compare numerically):

   - Branch (`A.Minibranch == 0`): `A.Patch <= B.Patch` when
     `B.Minibranch == 0`, or `A.Patch <= B.Minibranch` when `B.Minibranch > 0`.
   - Minibranch (`A.Minibranch > 0`): `A.Minibranch == B.Minibranch` and
     `A.Patch <= B.Patch`.

3. **Changelog Between Versions**: Run `version` on both versions for
   `commitHash` (`start_sha`, `end_sha`; avoids unfetched local Git tags and
   release branches), then run:

   ```bash
   curl -s \
     "https://chromium.googlesource.com/chromium/src/+log/{start_sha}..{end_sha}?format=JSON&n=10000" \
     | tail -n +2
   ```

4. **Milestone Branches & Deadlines**: Run `milestone` for `branches` (e.g.
   `refs/branch-heads/7103` to `latest-7103`) and `events`, then `version` on
   `latest-{branch}` (branch tip) or `latest-main` (trunk; changes after
   `branch_point` need a cherry-pick).

5. **Currently Serving Channel Releases**: Only when asked what versions are
   actively serving right now (`latest-*` returns the newest Git tag, which may
   be unshipped), query the
   [VersionHistory API](https://developer.chrome.com/docs/versionhistory/)
   (`{platform}`: `win`, `win64`, `win_arm64`, `mac`, `mac_arm64`, `linux`,
   `android`, `webview`, `ios`, `chromeos`, `all`; `{channel}`: `canary`, `dev`,
   `beta`, `stable`, `extended`, `all`):

   ```bash
   curl -s \
     "https://versionhistory.googleapis.com/v1/chrome/platforms/{platform}/channels/{channel}/versions/all/releases?filter=endtime=none,fraction%3E0"
   ```

   `fraction` is scoped per `fractionGroup`: on `win*`/`mac*`, `pinnable: false`
   is the consumer rollout while `pinnable: true` (`fraction: 1`) is for
   enterprise version-pinned clients.
