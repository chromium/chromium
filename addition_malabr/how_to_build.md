# How to build for You



## Enable the Debugging

### 1. Update the debug flag

at `out/Default/args.gn` update the flag `is_debug` `false` to `true`.

### 2. Regenerate Build Files:

```bash
gn gen out/Default
```

### 3. Build the Chromium

```bash
ninja -C out/Default chrome
```

You can also build only your target if you're testing an extension:

```bash
ninja -C out/Default extensions
```

### 4. Run Chrome with:

```bash
out/Default/chrome --enable-logging=stderr --v=1
```