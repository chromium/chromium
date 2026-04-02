---
name: network-traffic-annotations
description: Guide for writing and managing Network Traffic Annotations in Chromium. Use this skill when adding or updating code that makes network requests.
---

# Network Traffic Annotations

Network traffic annotations provide transparency into Chrome’s network
communication by documenting the intent, payload, and control mechanisms of each
network request.

## Where to add

Add annotations at the most rational point of origin for a request. This is
typically where:

1. The origin of the user’s intent or internal requirement is stated.
2. The controls (settings or policies) to stop or limit the request are
   enforced.
3. The data being sent is specified.

## Authoring Process

When adding a network annotation, you MUST follow these steps in sequence:

1. Verify pre-conditions.
2. Give the user an overview of the process.
3. Gather requirements: start by reading the .cc file. Anything you're not sure
   of, ask the user explicitly and directly. You MUST ask the user for anything
   you're not sure of.
4. Explicitly ask the user to verify the accuracy of annotation's content.
5. Write the annotation based on the requirements you've gathered
6. Run the `auditor.py` script
7. Ask the user to review the contents one last time before they upload it for
   review.

## Annotation Tag Content

Each annotation is defined using
`net::DefineNetworkTrafficAnnotation("unique_id", R"(...)")`. The second
argument is a text-encoded `NetworkTrafficAnnotation` protobuffer, as defined in
`chrome/browser/privacy/traffic_annotation.proto`.

To determine the annotation's contents, start by reading the source code of the
file the annotation is in. If you need more information, or you are unsure what
to enter for a particular field, you MUST ask the user for more information
before proceeding.

### Essential Semantics Fields

- **sender**: The component or feature triggering the request (e.g., "Safe
  Browsing").
- **description**: Plaintext explanation of the request and its value
  proposition. This is meant for a technical audience, but not Chrome/Chromium
  developers. Avoid obscure or internal code names.
- **trigger**: The specific user action that triggers the request.
- **user_data**: The nature of the data being sent (use enums from
  `chrome/browser/privacy/traffic_annotation.proto`).
- **destination**:
  - `GOOGLE_OWNED_SERVICE` for Google endpoints
  - `WEBSITE` for a website the user is visiting
  - `OTHER` for any other endpoint
    - If you use `OTHER`, explain it in the `destination_other` string field.
- **contacts**: A list of emails for points-of-contact (individuals, or a team
  alias). You MUST ask the user which email they want to use. `contacts` is a
  repeated field.
- **last_reviewed**: Date of last review in `YYYY-MM-DD` format. Use today's
  date, e.g. using the `date` command.

### Essential Policy Fields

- **cookies_allowed**: `YES` or `NO`.
- **setting**: How a user can enable/disable the feature in settings. If there
  is no setting, explain why.
- **chrome_policy**: The enterprise policy that disables this request, and what
  value to use to disable the request. Recently-added policies may need to be
  wrapped in `subProto1 { ... }` so `auditor.py` can parse them. You can find
  policy definitions in `components/policy/resources/templates/policies.yaml`
  and `components/policy/resources/templates/policy_definitions/`.
- **policy_exception_justification**: If no enterprise policy exists to disable
  this request, explain why.

The traffic annotation MUST contain either `chrome_policy` or
`policy_exception_justification`, but not both.

## Template

```cpp
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("...", R"(
          semantics {
            sender: "..."
            description: "..."
            trigger: "..."
            destination: WEBSITE/GOOGLE_OWNED_SERVICE/OTHER
            data: "..."
            user_data {
              type: ...
            }
            last_reviewed: "YYYY-MM-DD"
            internal {
              contacts {
                email: "..."
              }
              contacts {
                email: "..."
              }
            }
          }
          policy {
            cookies_allowed: NO/YES
            setting: "..."
            chrome_policy {
              [POLICY_NAME] {
                  [POLICY_NAME]: ...
              }
            }
            policy_exception_justification: "..."
          }
        )");
```

## Running the Auditor

After adding or updating an annotation, you MUST verify it using the
`auditor.py` script. Explain to the user what you're about to do, and why you're
doing it.

### 2. Build Chrome

You MUST ask the user which directory to use as the build path before
proceeding.

Ensure you have a fresh build of the `chrome` target. For instance with
`autoninja -C out/<build_path> chrome`, replacing `out/<build_path>` with the
build path.

### 3. Run the Auditor

Use the same `build_path` where you just built `chrome`.

```bash
vpython3 tools/traffic_annotation/scripts/auditor/auditor.py --build-path=out/<build_path>
```

#### Pre-conditions

The `auditor.py` script cannot run under these conditions. If any of these
conditions are true, `auditor.py` will fail with an explanation of why. If any
of these are tru, you MUST abort immediately and inform the user:

- You are not running inside a Git repository.
- You are not running on Linux or Windows.

If you abort, the user has two options:

- Patch their change into a Git repository (on Linux/Windows), so they can run
  `auditor.py` locally.
- Upload their CL to Gerrit, and do a CQ dry run.

Inform the user of their options, and don't do anything else.

### 4. Update Summary Files

The auditor will inform you if you need to update
`tools/traffic_annotation/summary/annotations.xml` or `grouping.xml`.

### 5. Update Platform List

For new annotations, `auditor.py` creates an entry in `annotations.xml`. The
entry is populated with a "default" list of target platforms, which may or may
not be accurate.

```xml
 <item id="..." ... os_list="linux,windows,android,chromeos" ... />
```

Update `os_list` to match the actual list of target platforms. It should be
based on BUILD.gn files; or, if you can't figure it out from BUILD.gn files, ask
the user directly.

The only valid platforms for `os_list` are:

- linux
- windows
- chromeos
- android

macOS and iOS are not a valid platforms in this context. If the user mentions
macOS or iOS, just ignore it.
