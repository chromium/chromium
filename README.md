
This repository is a chromium fork that has has been adapted to use the record/replay driver.

# Setting up builds

Only one build configuration is currently supported.

Install depot_tools per https://chromium.googlesource.com/chromium/src/+/master/docs/mac_build_instructions.md#Install

```
cd src
gn gen out/Release
gn args out/Release
```

Add the following settings:

```
is_debug = false
enable_nacl = false
```

Build:

```
autoninja -C out/Release chrome
```

# Merging from upstream

Because chromium's source is split across many git repositories, merging changes from upstream is tricky

Pull upstream changes and merge into master branch:

```
git checkout upstream
git pull https://github.com/chromium/chromium.git master
git push
git checkout master
git merge upstream
... fix merge conflicts ...
git commit -a
git push
```

Update other dependencies:

```
cd /path/to/chromium
gclient sync -D
```

gclient will change V8 to point back to the default google remote, erasing all record/replay changes.  Merge them back in:

```
cd v8
git remote set-url origin https://github.com/RecordReplay/v8
git pull
... fix merge conflicts ...
git commit -a
git push
```
