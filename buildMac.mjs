// Script used by buildkite to build Chromium for macOS in CI
import {
  spawnChecked,
  updateChromiumRepo,
} from "./replay_build_scripts/common.mjs";

spawnChecked("node", ["build.js"], { stdio: "inherit" });
