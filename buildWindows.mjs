// Script used by buildkite to build Chromium for Windows in CI
import { spawnChecked } from "./replay_build_scripts/common.mjs";

// TODO(dmiller): remove this hack when we switch to the new ci system
spawnChecked("git", ["apply", "replay_build_scripts/windows.patch"]);

try {
  spawnChecked("node", ["build.js"], { stdio: "inherit" });
} finally {
  spawnChecked("git", [
    "checkout",
    "media/audio/win/audio_low_latency_input_win.cc",
  ], { stdio: "inherit" });
}
