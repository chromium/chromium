---
name: chrome-releases
description: >-
  Queries Chrome commit, version, release, and milestone metadata. Use when
  finding the first landed Chrome version of a commit, checking channel releases
  across platforms, inspecting milestone schedules and branch refs, or checking
  version containment.
---

# Chrome Releases & Commit Tracker

Queries relationships between Chrome commits, versions, releases, and milestones
using the Developer Graph API via `chrome_releases.py`.

## Capabilities & Commands

### 1. Look Up a Commit (`commit`)

Returns Git metadata, associated Gerrit changes, derived commit relations
(cherry picks, reverts, relands), and the earliest version tag ("first landed"
version).

```bash
python3 agents/skills/chrome-releases/scripts/chrome_releases.py commit <COMMIT_HASH>
```

**Key Fields in Response:**

- `chromeMetadata.commitPosition`: The commit position number.
- `chromeMetadata.versions`: Earliest version tags containing this commit.
  - For Chromium commits: Contains the Chrome version (e.g. `136.0.7051.0`).
  - For sub-repo commits (e.g. V8): Contains the sub-repo version and the first
    Chromium version that rolled it in.
- `sourceCommits` / `derivedCommits`: Related commits (`CHERRY_PICK`, `REVERT`,
  `RELAND`, `DEPENDENCY_ADD`, `DEPENDENCY_REMOVE`).
- `gerritChanges`: Change number, Change-Id, host, project, and branch.

### 2. Look Up a Version (`version`)

Returns the Git commit hash, parent version, minibranch info, and first release
on each platform and channel.

```bash
python3 agents/skills/chrome-releases/scripts/chrome_releases.py version <VERSION_OR_ALIAS>
```

**Aliases:**

- `latest-main`: Latest version on the main trunk.
- `latest-{branch}`: Latest version on a release branch (e.g. `latest-7103`).
- `latest-{branch}_{minibranch}`: Latest version on a minibranch (e.g.
  `latest-7103_160`).

**Key Fields in Response:**

- `chromeMetadata.parentVersion`: Immediately preceding version in the version
  tree.
- `chromeMetadata.minibranch`: Patch number on the parent branch if this version
  is on a minibranch.
- `chromeMetadata.firstReleases`: First releases of this version by platform and
  channel (e.g. `mac/stable`, `android/beta`, `win/canary`), including release
  version and start timestamp.

### 3. Look Up a Milestone (`milestone`)

Returns the release branch and scheduled lifecycle event dates for a major
Chrome milestone.

```bash
python3 agents/skills/chrome-releases/scripts/chrome_releases.py milestone <MILESTONE>
```

**Key Fields in Response:**

- `branches`: Branch refs (e.g. `refs/branch-heads/6723`) and the main-branch
  divergence commit hash.
- `events`: Scheduled dates for milestone events:
  - `branch_point`: Branch creation date.
  - `final_beta_cut` / `stable_cut`: Code freeze/cut dates.
  - `earliest_beta` / `final_beta`: Beta rollout dates.
  - `early_stable`: Early stable rollout date (~0.5% users).
  - `stable_date`: Stable release date (100% rollout).
  - `stable_refresh_first`, `stable_refresh_second`, `stable_refresh_third`:
    Bi-weekly refresh dates.
  - `feature_freeze`, `late_stable_date`, `chromeos_*`: ChromeOS specific
    milestone events.

## Workflows

### 1. Find Where a Commit Landed and Released

1. Run `commit` with `<commit_hash>`.
2. Read `chromeMetadata.versions` to find the Chrome "First Landed" version
   (e.g. `136.0.7051.0`).
3. Run `version` with `<first_landed_version>` to read `firstReleases` for
   target platforms/channels.

### 2. Track a Subproject Commit (V8, Skia) into Chrome

1. Run `commit` with `<subproject_commit_hash>`.
2. Inspect `chromeMetadata.versions`. It lists:
   - The subproject version (e.g. V8 `12.0.0.0`).
   - The first Chrome version that included the dependency roll (e.g. Chrome
     `120.0.6099.0`).
3. Run `version` with the Chrome version to find the Chrome releases that
   include the roll.

### 3. Determine if Commit Version A is in Release Version B

Version numbers follow `Major.Minor.Build.Patch` (e.g. `136.0.7051.0`).

Use this rule to determine if version `A` is an ancestor of version `B`:

- **Main branch (`A.Patch == 0`)**: `A` is in `B` if `A.Build <= B.Build`.
- **Release branch (`A.Patch > 0`, no minibranch)**: `A` is in `B` if
  `A.Build == B.Build` and `A.Patch <= B.Patch`.
- **Minibranch (`A.Minibranch > 0`)**: `A` is in `B` if `B` is on the same
  minibranch (`A.Build == B.Build`, `A.Minibranch == B.Minibranch`, and
  `A.Patch <= B.Patch`).

### 4. Find the Release Branch and Latest Version for a Milestone

1. Run `milestone` with `<milestone>` to get the branch ref under `branches`
   (e.g. `refs/branch-heads/7103` -> branch number `7103`).
2. Run `version` with `latest-{branch_number}` (e.g. `latest-7103`) to find the
   latest version on that release branch.

### 5. Check Milestone Deadlines for a Pending Change

1. Call `version` with `latest-main` to see the current main trunk build number.
2. Call `milestone` with the upcoming `<milestone>` to read `events` dates
   (`branch_point`, `stable_cut`, `stable_date`).
3. If a change lands before the `branch_point` date, it lands in that milestone
   automatically. If it lands after, it requires a cherry-pick to
   `refs/branch-heads/{branch_number}`.
