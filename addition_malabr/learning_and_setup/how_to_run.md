# How to Run for You

## Enable the Debugging

### 1. Update the debug flag

at `out/Default/args.gn` update the flag `is_debug` `false` to `true`.

### 2. Regenerate Build Files:

```bash
gn gen out/Default
```

### 3. Build the Chromium

```bash
autoninja -C out/Default chrome
```

### 4. Run Chrome with:

```bash
out/Default/chrome --enable-logging=stderr --v=1
```