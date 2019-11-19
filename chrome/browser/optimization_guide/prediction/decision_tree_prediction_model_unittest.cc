// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/optimization_guide/prediction/decision_tree_prediction_model.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

std::unique_ptr<proto::PredictionModel> GetValidDecisionTreePredictionModel() {
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
  return prediction_model;
}

std::unique_ptr<proto::PredictionModel> GetValidEnsemblePredictionModel() {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      std::make_unique<proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);
  proto::Ensemble ensemble = proto::Ensemble();
  *ensemble.add_members()->mutable_submodel() =
      *GetValidDecisionTreePredictionModel()->mutable_model();

  *ensemble.add_members()->mutable_submodel() =
      *GetValidDecisionTreePredictionModel()->mutable_model();

  *prediction_model->mutable_model()->mutable_ensemble() = ensemble;
  return prediction_model;
}

TEST(DecisionTreePredictionModel, ValidDecisionTreeModel) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityLessThan) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::LESS_THAN);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityGreaterOrEqual) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::GREATER_OR_EQUAL);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityGreaterThan) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::GREATER_THAN);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
}

TEST(DecisionTreePredictionModel, MissingInequalityTest) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->Clear();

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, NoDecisionTreeThreshold) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()->clear_threshold();

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, EmptyTree) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()->mutable_decision_tree()->clear_nodes();

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, ModelFeatureNotInFeatureMap) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()->mutable_decision_tree()->clear_nodes();

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeMissingLeaf) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(1)
      ->mutable_leaf()
      ->Clear();

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeLeftChildIndexInvalid) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_left_child_id()
      ->set_value(3);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeRightChildIndexInvalid) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_right_child_id()
      ->set_value(3);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeWithLoopOnLeftChild) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  proto::TreeNode* tree_node =
      prediction_model->mutable_model()->mutable_decision_tree()->mutable_nodes(
          1);

  tree_node->mutable_node_id()->set_value(0);
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

  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(0);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeWithLoopOnRightChild) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  proto::TreeNode* tree_node =
      prediction_model->mutable_model()->mutable_decision_tree()->mutable_nodes(
          1);

  tree_node->mutable_node_id()->set_value(0);
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

  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(2);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(0);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, ValidEnsembleModel) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidEnsemblePredictionModel();

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, EnsembleWithNoMembers) {
  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetValidEnsemblePredictionModel();
  prediction_model->mutable_model()
      ->mutable_ensemble()
      ->mutable_members()
      ->Clear();

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  EXPECT_FALSE(model);
}

}  // namespace optimization_guide
