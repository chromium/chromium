// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_model.h"

#include <memory>

#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(PredictionModelTest, ValidPredictionModel) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      std::make_unique<proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);

  proto::DecisionTree decision_tree_model = proto::DecisionTree();
  decision_tree_model.set_weight(2.0);

  proto::TreeNode* tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(0);
  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(1);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_feature_id()
      ->mutable_id()
      ->set_value("agg1");
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::LESS_OR_EQUAL);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_threshold()
      ->set_float_value(1.0);

  tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(1);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      2.);

  tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(2);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      4.);

  *prediction_model->mutable_model()->mutable_decision_tree() =
      decision_tree_model;

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      optimization_guide::proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});

  EXPECT_EQ(1, model->GetVersion());
  EXPECT_EQ(2u, model->GetModelFeatures().size());
  EXPECT_TRUE(model->GetModelFeatures().count("agg1"));
  EXPECT_TRUE(model->GetModelFeatures().count(
      "CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE"));
}

TEST(PredictionModelTest, NoModel) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(PredictionModelTest, NoModelVersion) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(PredictionModelTest, NoModelType) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(PredictionModelTest, UnknownModelType) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_UNKNOWN);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(PredictionModelTest, MultipleModelTypes) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_UNKNOWN);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(PredictionModelTest, UnknownModelClientFeature) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);

  model_info->add_supported_model_features(
      optimization_guide::proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_UNKNOWN);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

}  // namespace optimization_guide
