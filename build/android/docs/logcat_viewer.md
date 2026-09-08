# Logcat Viewer

## What is it?

The logcat viewer is a web page that displays a logcat based on the options
selected by the user. It is designed to make test logs easier to analyze.

## Where is it?

Here is the URL to the logcat viewer:
https://luci-logdog-dev.appspot.com/static/logcat.html


## Where is the source code?

Here are the URLs to its source code:

* https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/logdog/appengine/cmd/coordinator/static/static/logcat.html

* https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/logdog/appengine/cmd/coordinator/static/static/css/logcat.css

* https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/logdog/appengine/cmd/coordinator/static/static/js/logcat.js

## How to deploy it?

After making some changes to the source code, you can make your changes go live
by running the following commands:

```sh
git clone https://chrome-internal.googlesource.com/infradata/gae
./scripts/promote.py luci-logdog --canary --commit
git cl upload
```

This deploys your changes to canary. You can then deploy your changes to stable
by running the following commands:

```sh
./scripts/promote.py luci-logdog --stable --commit
git cl upload
```

## How to file a bug?

Visit https://g-issues.chromium.org/issues/new?component=1925810
