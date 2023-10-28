// Script used by buildkite to build Chromium for Linux in CI
import path from "path";
import {
  spawnChecked,
  updateChromiumRepo,
} from "./replay_build_scripts/common.mjs";

const dockerArgs = [
  "run",
  "-e",
  "BUILDKITE",
  "-e",
  "BUILDKITE_BRANCH",
  "-e",
  "BUILDKITE_PIPELINE_DEFAULT_BRANCH",
  "-e",
  "LOCAL_DEVELOPER_BUILD_EXTENSION",
  "-e",
  "GOMA_SERVER_HOST=simpsonite.goma.engflow.com",
  "-e",
  "GOMACTL_USE_PROXY=false",
  "-e",
  "DRIVER_REVISION",
  "-v",
  `${path.join(process.env.HOME, "chromium")}:/chromium`,
  "-v",
  `${path.join(process.env.HOME, "depot_tools")}:/depot_tools`,
  "-v",
  `${path.join(
    process.env.HOME,
    ".goma_client_oauth2_config"
  )}:/home/ubuntu/.goma_client_oauth2_config`,
  "chromium-build-new",
];

spawnChecked("docker", dockerArgs, { stdio: "inherit" });
